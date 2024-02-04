#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include <string>
#include <sstream>
#include <iomanip>

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "cpp_redis/core/client.hpp"

const std::string &CORS_ORIGIN_ALLOW = "brian-ubuntu-22-pc-q35-ich9-2009";

bool isMeasure(std::string measureString);
void replaceChar(std::string &inputString, const char &searchChar, const char &replacementChar);
std::pair<std::string, std::string> splitMeasure(const std::string &valueMeasure);

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
  CROW_ROUTE(app, "/api/recipies").methods(crow::HTTPMethod::GET)
  ([&](){

    // Get the title of the recipes and their content
    std::future<cpp_redis::reply> rKeys = redisClient.keys("*");
    redisClient.sync_commit();
    rKeys.wait();
    // get the reply as an array
    const std::vector<cpp_redis::reply> resKeys = rKeys.get().as_array();

    std::vector<std::string> recipieNameVec(resKeys.size());

    // Get list of keys
    std::transform(resKeys.begin(), resKeys.end(), recipieNameVec.begin(), [](const cpp_redis::reply &rep) {return rep.as_string(); });

    // Loop through the recipie keys to make async requests to the database
    std::vector<std::pair<std::string, std::future<cpp_redis::reply>>> recipieRequestObjects {};
    try {
      for(std::string recipieKey : recipieNameVec) {
        // Get the ingredients from the db
        recipieRequestObjects.push_back(std::pair{recipieKey, redisClient.hgetall(recipieKey)});
      }

      redisClient.commit();
    } catch (const std::runtime_error &err) {
        return crow::response(500, "Internal Server Error");
    }
    

    // Go through the db replies and make a json object out of it to return
    std::vector<crow::json::wvalue> recipies;

    for(auto currRecipe = recipieRequestObjects.begin(); currRecipe != recipieRequestObjects.end(); currRecipe++) {
      // Wait for the future object to be received
      (*currRecipe).second.wait();
      // Get the array contents from the future object
      std::vector<cpp_redis::reply> rIngredients = (*currRecipe).second.get().as_array();
      std::vector<crow::json::wvalue> wIngredients = {};

      // Loop through the list of ingredients returned from the cpp redis call and add them to a JSON object
      for(int i = 0; i < rIngredients.size(); i += 2) {
        if(i >= rIngredients.size())
          break;

        if(!rIngredients[i].is_string() || !rIngredients[i+1].is_string())
          return crow::response(500, "Internal Server Error");

        const std::string ingredientName = rIngredients[i].as_string();
        // Split the value from the measure
        const std::pair<std::string, std::string> ingredientValue = splitMeasure(rIngredients[i+1].as_string());
        
        // Create the ingredient object
        crow::json::wvalue ingredientObject {};
        ingredientObject["ingredientName"] = ingredientName;
        ingredientObject["ingredientNumber"] = ingredientValue.first;
        ingredientObject["ingredientMeasurement"] = ingredientValue.second;

        wIngredients.push_back(ingredientObject);

      }

      // Give the name of the meal and add the ingredients list
      crow::json::wvalue wRecipie {{"mealName", (*currRecipe).first}};
      wRecipie["ingredientsList"] = crow::json::wvalue {wIngredients};

      //Add the recipie to the list of recipies
      recipies.push_back(wRecipie);
    }

    crow::response response(200, crow::json::wvalue{{"data", recipies}});
    response.add_header("Access-Control-Allow-Origin", "*");
    return response;
  });

  // Get a single recipe
  // CROW_ROUTE(app, "/api/recipies/<string>")
  // ([&](std::string recipeName){
    
  //   // Get the recipe and the content
  //   std::future<cpp_redis::reply> rKeys = redisClient.hgetall("recipeName");
  //   redisClient.sync_commit();
  //   rKeys.wait();
  //   // These are both replies
  //   std::vector<cpp_redis::reply> resTitles = rKeys.get().as_array();

  //   // Convert into json
  //   crow::json::wvalue recipe{
  //     {"title", resTitles},
  //     {"content", resContent}
  //   };

  //   return crow::json::wvalue{{"data", recipe}};
  // });

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
      //replaceChar(body["mealName"].s(), " ", "-"); // Remove the whitespace from it
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

        //convert double to string
        std::ostringstream oss;
        oss << std::setprecision(2) << std::noshowpoint << ingredientNumber;
        std::string ingredientNumberString = oss.str();

        // Test if the measure is an expected measure
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
          redisClient.hsetnx(mealName, ingredientName, ingredientNumberString + ingredientMeasurement, [&](cpp_redis::reply &reply) {
            if (!reply.is_integer()) {
              throw std::runtime_error("Redis did not return a number");
            }
            if (reply) {
              CROW_LOG_DEBUG << "Ingredient " << ingredientName << "add to " << mealName;
            }
            
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

    crow::response response(200, "Recipe added!");
    response.add_header("Access-Control-Allow-Origin", "*");
    return response;
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

std::pair<std::string, std::string> splitMeasure(const std::string &valueMeasure) {
  std::string ret = valueMeasure;
  
  if(valueMeasure.substr(valueMeasure.size()-2, 2) == "oz") {
    ret.resize(valueMeasure.size()-2);
    return std::make_pair(ret, "oz");
  }
  else if (valueMeasure.substr(valueMeasure.size()-3, 3) == "lbs") {
    ret.resize(valueMeasure.size()-3);
    return std::make_pair(ret, "lbs");
  }
  else if (valueMeasure.substr(valueMeasure.size()-3, 3) == "cup") {
    ret.resize(valueMeasure.size()-3);
    return std::make_pair(ret, "cup");
  }
  else if (valueMeasure.substr(valueMeasure.size()-3, 3) == "tsp") {
    ret.resize(valueMeasure.size()-3);
    return std::make_pair(ret, "tsp");
  }
  else if (valueMeasure.substr(valueMeasure.size()-4, 4) == "tbps") {
    ret.resize(valueMeasure.size()-4);
    return std::make_pair(ret, "tbps");
  }

  return {};
}

bool isMeasure(std::string measureString) {
  std::set<std::string> measures = {"lbs", "oz", "cup", "tbps", "tsp"};

  return measures.count(measureString) ? true : false;
}

// void replaceChar(std::string &inputString, const char &searchChar, const char &replacementChar) {
  
//   for(auto i = inputString.begin(); i < inputString.end(); i++) {
//     if(*i == searchChar)
//       inputString.replace((i-inputString.begin()), 1, "-");
//     }
//   }
  
//   return inputString;
// }