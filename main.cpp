#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include <string>

#include "crow.h"
#include "cpp_redis/core/client.hpp"

bool isMeasure(std::string measureString);

int main() {
  const std::string REDIS_IP_ADDR {"10.8.29.32"};
  const std::size_t REDIS_PORT {6379};
  //initialize
  crow::SimpleApp app;
  // Set log level
  app.loglevel(crow::LogLevel::Debug);

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
    std::future<cpp_redis::reply> rKeys = redisClient.keys("*");
    redisClient.sync_commit();
    rKeys.wait();
    // These are both replies
    auto resKeys = rKeys.get().as_array();

    std::vector<std::string> recipieVec(resKeys.size());

    // Map the responses into the cpp vector
    std::transform(resTitles.begin(), resTitles.end(), recipieVec.begin(), [](const cpp_redis::reply &rep) {return rep.as_string(); });

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
    crow::json::rvalue body = crow::json::load(req.body);
    CROW_LOG_DEBUG << "Got the body";
    if(!body)
      return crow::response(400, "Invalid body");

    std::string mealName;
    std::vector<crow::json::rvalue> ingredientList;

    // Get the title and the body of the response
    try {
      mealName = body["mealName"].s(); // Get the string form of the title
      CROW_LOG_DEBUG << "Meal name: " << mealName;

      // Get the list
      // Need to separate ingredientListBody from ingredientList, because, you can't chain functions for constant values?
      const crow::json::rvalue ingredientList = body["ingredientsList"];
      CROW_LOG_DEBUG << "Ingredient list body type: "<< crow::json::get_type_str(ingredientList.t());

      std::string ingredientName, ingredientNumberString, ingredientMeasurement;
      double ingredientNumber;

      // Get the ingredient list
      for(crow::json::rvalue *i = ingredientList.begin(); i != ingredientList.end(); i++) {
        std::vector<crow::json::rvalue> ingredientItem = (*i).lo(); // Get vector of ingredients
        //CROW_LOG_DEBUG << "Ingredient list type: " << crow::json::get_type_str(ingredientItem);


        // Assign values to variables
        ingredientName = {ingredientItem[0].s()};
        ingredientNumber = {ingredientItem[1].d()};
        ingredientMeasurement = {ingredientItem[2].s()};

        //number check
        std::string ingredientNumberString = std::to_string(ingredientNumber);

        // Is a true measure
        if(!isMeasure(ingredientMeasurement)) {
          CROW_LOG_DEBUG << "Invalid Measurement " << ingredientMeasurement << " at index: " << i - ingredientList.begin();
          return crow::response(400, "Invalid Ingredient Measure ");
        }

        CROW_LOG_DEBUG << "Ingredient Name: " << ingredientName;
        CROW_LOG_DEBUG << "Ingredient Amount: " << ingredientNumber;
        CROW_LOG_DEBUG << "Ingredient Amount (string version): " << ingredientNumberString;
        CROW_LOG_DEBUG << "Ingredient Measurement: " << ingredientMeasurement;


        // Queue data to the database
        try {
          redisClient.hsetnx(mealName, ingredientName, ingredientNumberString + ingredientMeasurement, [](cpp_redis::reply &reply) {
            if (!reply.is_integer()) {
              throw std::runtime_error("Redis did not return a number");
            }
            
            CROW_LOG_DEBUG << "Ingredient in Redis: " << reply;
          });

          redisClient.sync_commit();

        } catch (const std::runtime_error &err) {
          return crow::response(500, "Internal Server Error");
        }
      }

      // Sync data to the database
      try {

          redisClient.sync_commit();

        } catch (const std::runtime_error &err) {
          return crow::response(500, "Internal Server Error");
        }
      
    } catch (const std::runtime_error &err) {
      CROW_LOG_DEBUG << "Failure: " << err.what();
      return crow::response(400, "Invalid body");
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

bool isMeasure(std::string measureString) {
  std::set<std::string> measures = {"lbs", "oz", "cup", "tbps", "tsp"};

  return measures.count(measureString) ? true : false;
}