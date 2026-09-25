#include "order_book.hpp"

#include <drogon/drogon.h>
#include <trantor/net/EventLoopThread.h>
#include <drogon/PubSubService.h>
#include <drogon/WebSocketController.h>

#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// web socket code

struct MarketDataSubscription {
    drogon::SubscriberID id;
};

class MarketDataHub {
public:
    using MessageHandler = drogon::PubSubService<std::string>::MessageHandler;

    drogon::SubscriberID subscribe(MessageHandler handler) {
        return publisher_.subscribe(
            "marketdata",
            std::move(handler)
        );
    }

    void unsubscribe(drogon::SubscriberID id) {
        publisher_.unsubscribe("marketdata", id);
    }

    void publish(const std::string& message) {
        publisher_.publish("marketdata", message);
    }

private:
    drogon::PubSubService<std::string> publisher_;
};

// exchnage service code
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

// json helpers

// converts a book snapshot to a json for the network
Json::Value snapshotJson(const BookSnapshot& snapshot) {

    Json::Value json;
    // init bids arr
    json["bids"] = Json::Value(Json::arrayValue);

    // init asks arr
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
    json["remaining_quantity"] = Json::Int64(result.remaining_quantity);
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

// json helper for the websocket
std::string jsonString(const Json::Value& json) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return Json::writeString(writer, json);
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

// api code
class ExchangeApi : public drogon::HttpController<ExchangeApi, false> {

public:
    ExchangeApi(
        std::shared_ptr<ExchangeService> service,
        std::shared_ptr<MarketDataHub> hub
    ) : service_(std::move(service)), hub_(std::move(hub)) {
    }

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ExchangeApi::getBook, "/book", drogon::Get);
    ADD_METHOD_TO(ExchangeApi::postOrder,"/orders", drogon::Post);
    ADD_METHOD_TO(ExchangeApi::deleteOrder, "/orders/{id}", drogon::Delete);
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

    void postOrder(
        const drogon::HttpRequestPtr& request,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    ) {
        
        //parse the request body as a json
        const auto json = request->getJsonObject();

        //if body not a json throw
        if (!json || !json->isObject()) {
            callback(errorResponse(
                drogon::k400BadRequest,
                "Request body must be a JSON object"
            ));
            return;
        }

        //make sure side is in json, and make sure its corresponding val is a string
        if (!json->isMember("side") ||
            !(*json)["side"].isString()) {
            callback(errorResponse(
                drogon::k400BadRequest,
                "side must be \"buy\" or \"sell\""
            ));
            return;
        }

        if (!json->isMember("price") ||
            !(*json)["price"].isInt64()) {
            callback(errorResponse(
                drogon::k400BadRequest,
                "price must be an integer"
            ));
            return;
        }

        if (!json->isMember("quantity") ||
            !(*json)["quantity"].isInt64()) {
            callback(errorResponse(
                drogon::k400BadRequest,
                "quantity must be an integer"
            ));
            return;
        }

        const std::string side_text = (*json)["side"].asString();

        Side side;
        if (side_text == "buy") {
            side = Side::Buy;
        } else if (side_text == "sell") {
            side = Side::Sell;
        } else {
            callback(errorResponse(
                drogon::k400BadRequest,
                "side must be \"buy\" or \"sell\""
            ));
            return;
        }

        const std::int64_t price = (*json)["price"].asInt64();
        const std::int64_t quantity =
            (*json)["quantity"].asInt64();

        service_->enqueue(
            [side,
            price,
            quantity,
            hub = hub_,
            reply = std::move(callback)](OrderBook& book) {
                try {
                    const SubmitResult result =
                        book.submit(side, price, quantity);

                    Json::Value event;
                    event["type"] = "snapshot";
                    event["book"] = snapshotJson(book.snapshot(5));
                    event["trades"] = Json::Value(Json::arrayValue);

                    for (const Trade& trade : result.trades) {
                        Json::Value entry;
                        entry["price"] = Json::Int64(trade.price);
                        entry["quantity"] = Json::Int64(trade.quantity);
                        event["trades"].append(entry);
                    }

                    // tell everyone abt it!
                    hub->publish(jsonString(event));

                    auto response =
                        drogon::HttpResponse::newHttpJsonResponse(
                            submitResultJson(result)
                        );
                    response->setStatusCode(drogon::k201Created);
                    reply(response);
                } catch (const std::invalid_argument& error) {
                    reply(errorResponse(
                        drogon::k400BadRequest,
                        error.what()
                    ));
                } catch (const std::exception& error) {
                    LOG_ERROR << error.what();

                    reply(errorResponse(
                        drogon::k500InternalServerError,
                        "Unable to submit order"
                    ));
                }
            }
        );
    }

    void deleteOrder(
        const drogon::HttpRequestPtr&,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        std::uint64_t order_id
    ) {
        service_->enqueue(
            [order_id,
            hub = hub_,
            reply = std::move(callback)](OrderBook& book) {
                try {
                    if (!book.cancel(order_id)) {
                        reply(errorResponse(
                            drogon::k404NotFound,
                            "Resting order not found"
                        ));
                        return;
                    }

                    Json::Value event;
                    event["type"] = "snapshot";
                    event["book"] = snapshotJson(book.snapshot(5));
                    event["trades"] = Json::Value(Json::arrayValue);

                    // tell everyone abt it!
                    hub->publish(jsonString(event));

                    Json::Value json;
                    json["order_id"] = Json::UInt64(order_id);
                    json["cancelled"] = true;

                    reply(drogon::HttpResponse::newHttpJsonResponse(json));

                } catch (const std::exception& error) {
                    LOG_ERROR << error.what();

                    reply(errorResponse(
                        drogon::k500InternalServerError,
                        "Unable to cancel order"
                    ));
                }
            }
        );
    }

private:
    std::shared_ptr<ExchangeService> service_;
    std::shared_ptr<MarketDataHub> hub_;
};

class MarketDataWebSocket : public drogon::WebSocketController<MarketDataWebSocket, false> {
public:
    MarketDataWebSocket(
        std::shared_ptr<ExchangeService> service,
        std::shared_ptr<MarketDataHub> hub
    ) : service_(std::move(service)), hub_(std::move(hub)) {}

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/marketdata", drogon::Get);
    WS_PATH_LIST_END

    void handleNewConnection(
        const drogon::HttpRequestPtr&,
        const drogon::WebSocketConnectionPtr& connection
    ) override {
        const drogon::SubscriberID id = hub_->subscribe(
            [connection](
                const std::string&,
                const std::string& message
            ) {
                connection->send(message);
            }
        );

        connection->setContext(
            std::make_shared<MarketDataSubscription>(MarketDataSubscription{id})
        );

        service_->enqueue(
            [connection](OrderBook& book) {
                Json::Value event;
                event["type"] = "snapshot";
                event["book"] = snapshotJson(book.snapshot(5));

                connection->send(jsonString(event));
            }
        );
    }

    void handleConnectionClosed(
        const drogon::WebSocketConnectionPtr& connection
    ) override {
        const auto subscription =
            connection->getContext<MarketDataSubscription>();

        if (subscription) {
            hub_->unsubscribe(subscription->id);
        }
    }

    void handleNewMessage(
        const drogon::WebSocketConnectionPtr&,
        std::string&&,
        const drogon::WebSocketMessageType&
    ) override {

    }

private:
    std::shared_ptr<ExchangeService> service_;
    std::shared_ptr<MarketDataHub> hub_;
};

int main() {
    auto service = std::make_shared<ExchangeService>();
    auto hub = std::make_shared<MarketDataHub>();
    auto api = std::make_shared<ExchangeApi>(service, hub);
    auto market_data = std::make_shared<MarketDataWebSocket>(service, hub);

    drogon::app().registerController(api);
    drogon::app().registerController(market_data);
    drogon::app().addListener("127.0.0.1", 8080);
    drogon::app().setThreadNum(2);
    drogon::app().run();
}
