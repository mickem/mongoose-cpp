#ifndef _MSC_VER
#include <unistd.h>
#else 
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#endif
#include <string>
#include <string.h>
#include <iostream>
#include "Server.h"
#include "Utils.h"

using namespace std;
using namespace Mongoose;

static int getTime()
{
#ifdef _MSC_VER
    time_t ltime;
    time(&ltime);
    return ltime;
#else
    return time(NULL);
#endif
}

/**
 * The handlers below are written in C to do the binding of the C mongoose with
 * the C++ API
 */
static void event_handler(struct mg_connection *connection, int ev, void *ev_data)
{
	struct http_message *message = (struct http_message *) ev_data;
    Server *server = (Server *)connection->user_data;

	if (server != NULL) {
		if (ev == MG_EV_HTTP_REQUEST) {
#ifndef NO_WEBSOCKET
			if (connection->is_websocket) {
				server->_webSocketReady(connection, message);
			}
#endif
			if (!server->_handleRequest(connection, message)) {
				static struct mg_serve_http_opts s_http_server_opts;

				s_http_server_opts.document_root = "C:\\source\\build\\x64\\dev\\web";  // Serve current directory
				s_http_server_opts.enable_directory_listing = "yes";

				mg_serve_http(connection, message, s_http_server_opts);
			}
		}
    }
}

#ifndef NO_WEBSOCKET
static int iterate_callback(struct mg_connection *connection)
{
    if (connection->is_websocket && connection->content_len) {
        Server *server = (Server *)connection->server_param;
        server->_webSocketData(connection, string(connection->content, connection->content_len));
    }

    return 1;
}
#endif

static void *server_poll(void *param)
{
    Server *server = (Server *)param;
    server->poll();

    return NULL;
}

namespace Mongoose
{
    Server::Server(const char *port_, const char *documentRoot)
        :  stopped(false)
		, destroyed(true)
		, port(port_)
#ifndef NO_WEBSOCKET 
        , websockets(NULL)
#endif

    {
		//memset(&mgr, 0, sizeof(mgr));
		mgr.user_data = this;
		mg_mgr_init(&mgr, NULL);
		memset(&opts, 0, sizeof(opts));
        optionsMap["document_root"] = string(documentRoot);
    }

    Server::~Server()
    {
        stop();
		vector<Controller *>::iterator it;
		for (it = controllers.begin(); it != controllers.end(); it++) {
			delete (*it);
		}
    }

	void Server::setSsl(const char *certificate) {
		opts.ssl_cert = certificate;
	}


    void Server::start()
    {
		const char *err = "";
		opts.error_string = &err;
		opts.user_data = this;
		server_connection = mg_bind_opt(&mgr, port.c_str(), event_handler, opts);
		if (server_connection == NULL) {
			throw mongoose_exception(err);
		}
		mg_set_protocol_http_websocket(server_connection);


        // size_t size = optionsMap.size()*2+1;

//             map<string, string>::iterator it;
//             for (it=optionsMap.begin(); it!=optionsMap.end(); it++) {
//                 const char* err = mg_set_option(server, (*it).first.c_str(), (*it).second.c_str());
// 				if (err)
// 					throw string("Failed to set " + (*it).first + ": " + err);
// 			}

        stopped = false;

        mg_start_thread(server_poll, this);
    }

    void Server::poll()
    {
		if (!stopped)
			destroyed = false;
        // unsigned int current_timer = 0;
        while (!stopped) {
			mg_mgr_poll(&mgr, 1000);
#ifndef NO_WEBSOCKET
            mg_iterate_over_connections(server, iterate_callback, &current_timer);
#endif
        }

        destroyed = true;
		mg_mgr_free(&mgr);
    }

    void Server::stop()
    {
        stopped = true;
        while (!destroyed) {
            Utils::xsleep(100);
        }
    }

    void Server::registerController(Controller *controller)
    {
        controller->setSessions(&sessions);
        controller->setServer(this);
        controller->setup();
        controllers.push_back(controller);
    }

#ifndef NO_WEBSOCKET
    void Server::_webSocketReady(struct mg_connection *conn)
    {
        WebSocket *websocket = new WebSocket(conn);
        websockets.add(websocket);
        websockets.clean();

        vector<Controller *>::iterator it;
        for (it=controllers.begin(); it!=controllers.end(); it++) {
            (*it)->webSocketReady(websocket);
        }
    }

    int Server::_webSocketData(struct mg_connection *conn, string data)
    {
        WebSocket *websocket = websockets.getWebSocket(conn);

        if (websocket != NULL) {
            websocket->appendData(data);

            string fullPacket = websocket->flushData();
            vector<Controller *>::iterator it;
            for (it=controllers.begin(); it!=controllers.end(); it++) {
                (*it)->webSocketData(websocket, fullPacket);
            }

            if (websocket->isClosed()) {
                websockets.remove(websocket);
                return 0;
            } else {
                return -1;
            }
        } else {
            return 0;
        }
    }
#endif

    bool Server::_handleRequest(struct mg_connection *connection, struct http_message *message)
    {
        Request request(connection, message);

        Response *response = handleRequest(request);

        if (response == NULL) {
            return false;
        } else {
            request.writeResponse(response);
            delete response;
            return true;
        }
    }

    bool Server::handles(string method, string url)
    {
#ifndef NO_WEBSOCKET
        if (url == "/websocket") {
            return true;
        }
#endif

        vector<Controller *>::iterator it;
        for (it=controllers.begin(); it!=controllers.end(); it++) {
            if ((*it)->handles(method, url)) {
                return true;
            }
        }

        return false;
    }

    Response *Server::handleRequest(Request &request)
    {
        Response *response;
        vector<Controller *>::iterator it;

        for (it=controllers.begin(); it!=controllers.end(); it++) {
            Controller *controller = *it;
            response = controller->process(request);

            if (response != NULL) {
                return response;
            }
        }

        return NULL;
    }

    void Server::setOption(string key, string value)
    {
        optionsMap[key] = value;
    }

#ifndef NO_WEBSOCKET
    WebSockets &Server::getWebSockets()
    {
        return websockets;
    }
#endif

}
