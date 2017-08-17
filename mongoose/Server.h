#ifndef _MONGOOSE_SERVER_H
#define _MONGOOSE_SERVER_H

#include <vector>
#include <iostream>
#include <mongoose.h>
#include "Request.h"
#include "Response.h"
#include "Controller.h"
#ifndef NO_WEBSOCKET
#include "WebSocket.h"
#include "WebSockets.h"
#endif
#include "Mutex.h"
#include "Sessions.h"

using namespace std;

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose
{

	class mongoose_exception : public std::exception {
	private:
		std::string error;
	public:
		mongoose_exception(std::string error) : error(error) {};
		~mongoose_exception() throw() {}

		const char* what() const throw() {
			return error.c_str();
		}
		std::string reason() const throw() {
			return error;
		}
	};

    class Server
    {
        public:
            /**
             * Constructs the server
             *
             * @param int the number of the port to listen to
             * @param string documentRoot the root that should be used for static files
             */
            Server(const char *port = "80", const char *documentRoot = "www");
            virtual ~Server();

            /**
             * Runs the Mongoose server
             */
            void start();

            /**
             * Stops the Mongoose server
             */
            void stop();

            /**
             * Register a new controller on the server
             *
             * @param Controller* a pointer to a controller
             */
            void registerController(Controller *);

            /**
             * Internally used to process a request
             *
             * @param struct mg_connection* the mongoose connection
             */
            bool _handleRequest(struct mg_connection *connection, struct http_message *message);

            /**
             * Internally used to process a file upload
             *
             * @param struct mg_connection* the mongoose conneciton
             * @param const char * the name of the uploaded file
             */
            void _upload(struct mg_connection *conn, const char *fileName);

            /**
             * Handles a web socket connection
             *
             * @param struct mg_connection* the mongoose connection with the client
             */
            void _webSocketReady(struct mg_connection *conn);

            /**
             * Handles web sockets data
             *
             * @param struct mg_connection* the mongoose connection
             * @param int flags the data flags
             * @param char* the data
             * @param size the data size
             *
             * @return int if we have to keep the connection opened
             */
            int _webSocketData(struct mg_connection *conn, string data);

            /**
             * Process the request by controllers
             *
             * @param Request the request
             *
             * @return Response the response if one of the controllers can handle it,
             *         NULL else
             */
            Response *handleRequest(Request &request);

			

			/**
			* Setup the mongoose ssl options section
			*
			* @param certificate the name of the certificate to use
			*/
			void setSsl(const char *certificate);
            /**
             * Sets a mongoose extra option
             *
             * @param string the name of the option
             * @param string the value of the option
             */
            void setOption(string key, string value);

#ifndef NO_WEBSOCKET
            /**
             * Returns the WebSockets container
             *
             * @return WebSockets the web sockets container
             */
            WebSockets &getWebSockets();
#endif

            /**
             * Print statistics
             */
            void printStats();

            /**
             * Polls the server
             */
            void poll();

            /**
             * Does the server handles url?
             */
            bool handles(string method, string url);

        protected:
			std::string port;
			struct mg_bind_opts opts;
			struct mg_mgr mgr;
            volatile bool stopped;
            volatile bool destroyed;
            Sessions sessions;
            //Mutex mutex;
            map<string, string> optionsMap;
            struct mg_connection *server_connection;

#ifndef NO_WEBSOCKET
            WebSockets websockets;
#endif

            vector<Controller *> controllers;
    };
}

#endif
