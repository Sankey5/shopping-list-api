#include <iostream>
#include <vector>
#include <utility>
#include <string>

#include "crow.h"
#include "cpp_redis/core/client.hpp"

int main() {
  const std::string REDIS_IP_ADDR {"10.8.29.32"};
  const std::size_t REDIS_PORT {6379};
  //initialize
  crow::SimpleApp app;

  //cpp_redis client
  cpp_redis::client redisClient;
  redisClient.connect(REDIS_IP_ADDR, REDIS_PORT, nullptr, 0, 0, 0);

  // ---------- TESTING SECTION ----------

  CROW_ROUTE(app,"/health")
  ([]()
    {return "Working fine...";}
  );
  
  // ---------- GET SECTION ----------

  // Get all recipies
  CROW_ROUTE(app, "/api/recipies")
  ([&](){

    // Get the title of the recipes and their content
    auto rTitles = redisClient.lrange("title", 0, -1);
    auto rContent = redisClient.lrange("content", 0, -1);
    redisClient.sync_commit();
    rTitles.wait();
    rContent.wait();
    // These are both replies
    auto resTitles = rTitles.get().as_array();
    auto resContent = rContent.get().as_array();

    std::vector<std::string> titles(resTitles.size()), contents(resContent.size());

    // Map the responses into the cpp vector
    std::transform(resTitles.begin(), resTitles.end(), titles.begin(), [](const cpp_redis::reply &rep) {return rep.as_string(); });
    std::transform(resContent.begin(), resContent.end(), contents.begin(), [](const cpp_redis::reply &rep) {return rep.as_string(); });

    // Convert into json
    std::vector<crow::json::wvalue> recipies;
    for (int i=0; i < titles.size(); i++) {
      recipies.push_back(crow::json::wvalue{
        {"title", titles[i]},
        {"content", contents[i]}
      });
    }

    return crow::json::wvalue{{"data", recipies}};
  });

  // Get a single recipe
  CROW_ROUTE(app, "/api/recipies/<int>")
  ([&](int recipeIndex){

    // Get the title of the recipe and the content
    auto rTitle = redisClient.lindex("title", recipeIndex);
    auto rContent = redisClient.lindex("content", recipeIndex);
    redisClient.sync_commit();
    rTitle.wait();
    rContent.wait();
    // These are both replies
    auto resTitles = rTitle.get().as_string();
    auto resContent = rContent.get().as_string();

    // Convert into json
    crow::json::wvalue recipe{
      {"title", resTitles},
      {"content", resContent}
    };

    return crow::json::wvalue{{"data", recipe}};
  });

  // ---------- CREATE SECTION ----------

  // Create a recipe
  CROW_ROUTE(app,"/api/recipies").methods(crow::HTTPMethod::POST)
  ([&](const crow::request &req) {
    auto body = crow::json::load(req.body);
    if(!body)
      return crow::response(400, "Invalid body");

    std::string title, content;

    // Get the title and the body of the response
    try {
      title = body["title"].s(); // Get the string form of the title
      content = body["content"].s(); // Get the string form of the body of the response
    } catch (const std::runtime_error &err) {
      return crow::response(400, "Invalid body");
    }

    // Send the data to the database
    try {
      //redisClient.lpush(""); // Add 
      //redisClient.sync_commit(); // Push the changes made to the database
    } catch (const std::runtime_error &err) {
      return crow::response(500, "Internal Server Error");
    }

    return crow::response(200, "Recipe added!");
  });

  // ---------- DELETE SECTION ----------

  // Delete a recipe

  // CROW_ROUTE(app,"/api/recipies/<int>").methods(crow::HTTPMethod::DELETE)
  // ([](int recipeIndex) {

  //   // Check if the index exists


  //   redisClient.sync_commit();
  //   rTitle.wait();
  //   rContent.wait();
  //   // These are both replies
  //   auto resTitles = rTitle.get().as_string();
  //   auto resContent = rContent.get().as_string();

  //   // Convert into json
  //   std::vector<crow::json::wvalue> recipie = 
  //   {
  //     {"title", resTitles},
  //     {"content", resContent}
  //   };

  //   return crow::json::wvalue{{"data", recipie}};
  // });

  app.port(18080).multithreaded().run();
}
