#include "order_book.hpp"

#include <drogon/drogon.h>
#include <trantor/net/EventLoopThread.h>

#include <exception>
#include <functional>
#include <memory>
#include <utility>
#include <string>

class ExchangeService {
public:
    ExchangeService() {
        worker_.run();
    }

    void enqueue(std::function<void(OrderBook&)> work) {
        worker_.getLoop()->queueInLoop(
            [this, work = std::move(work)]() {
                work(book_);
            }
        );
    }
private:
    OrderBook book_;
    trantor::EventLoopThread worker_;
};

// converts a book snapshot to a json for the network
Json::Value snapshotJson(const BookSnapshot& snapshot) {
    Json::Value json;
    json["bids"] = Json::Value(Json::arrayValue);
    json["asks"] = Json::Value(Json::arrayValue);

    for (const PriceLevel& level : snapshot.bids) {
        Json::Value entry;
        entry["price"] = Json::Int64(level.price);
        entry["quantity"] = Json::Int64(level.quantity);
        json["bids"].append(entry);
    }

    for (const PriceLevel& level : snapshot.asks) {
        Json::Value entry;
        entry["price"] = Json::Int64(level.price);
        entry["quantity"] = Json::Int64(level.quantity);
        json["asks"].append(entry);
    }

    return json;
}

Json::Value submitResultJson(const SubmitResult& result) {
    Json::Value json;
    json["order_id"] = Json::UInt64(result.order_id);
    json["remaining_quantity"] = Json::UInt64(result.remaining_quantity);
    json["trades"] = Json::Value(Json::arrayValue);

    for (const Trade& trade : result.trades) {
        Json::Value entry;
        entry["buy_order_id"] = Json::UInt64(trade.buy_order_id);
        entry["sell_order_id"] = Json::UInt64(trade.sell_order_id);
        entry["price"] = Json::Int64(trade.price);
        entry["quantity"] = Json::Int64(trade.quantity);
        json["trades"].append(entry);
    }

    return json;
}

drogon::HttpResponsePtr errorResponse(
    drogon::HttpStatusCode status,
    const std::string& message
) {
    Json::Value json;
    json["error"] = message;

    auto response =
        drogon::HttpResponse::newHttpJsonResponse(json);
    response->setStatusCode(status);
    return response;
}

// exchange api inherited from drogon http controller
class ExchangeApi : public drogon::HttpController<ExchangeApi, false> {

public:
    explicit ExchangeApi(std::shared_ptr<ExchangeService> service)
        : service_(std::move(service)) {
    }

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ExchangeApi::getBook, "/book", drogon::Get);
    METHOD_LIST_END

    void getBook(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    ) {
        service_->enqueue(
            [callback = std::move(callback)](OrderBook& book) {
                drogon::HttpResponsePtr response;

                try {
                    response = drogon::HttpResponse::newHttpJsonResponse(
                        snapshotJson(book.snapshot(5))
                    );
                } catch (const std::exception& error) {
                    LOG_ERROR << error.what();

                    Json::Value json;
                    json["error"] = "Unable to produce book snapshot";
                    response =
                        drogon::HttpResponse::newHttpJsonResponse(json);
                    response->setStatusCode(
                        drogon::k500InternalServerError
                    );
                }

                callback(response);
            }
        );
    }

private:
    std::shared_ptr<ExchangeService> service_;
};

int main() {
    auto service = std::make_shared<ExchangeService>();
    auto api = std::make_shared<ExchangeApi>(service);

    drogon::app().registerController(api);
    drogon::app().addListener("127.0.0.1", 8080);
    drogon::app().setThreadNum(2);
    drogon::app().run();
}