#include <mysql++.h>
#include <my_global.h>
#include <mysql.h>
#include <ctime>
#include <csignal>
#include <cstdlib>

#include <iostream>
#include <iomanip>

#include "server.h"

using namespace std;

#define DB_NAME "dns_resolve_stats"
#define DB_HOST "localhost"
#define DB_USR "root"
#define DB_PWD ""

#define MAX_SERVERS 10
#define MAX_SERVERS_STR 30
char server_strs[][MAX_SERVERS_STR] = { 
    "google.com",
    "facebook.com",
    "youtube.com",
    "yahoo.com",
    "live.com",
    "wikipedia.org",
    "baidu.com",
    "blogger.com",
    "msn.com",
    "qq.com"
};

void usage() {
    cerr << "Usage: ./dns_resolve_stats <time: int(1-1000)> <unit: 's' for secs 'm' for milisecs> <verbose: 'v'>" << endl;
    cerr << "examples: " << endl;
    cerr << "          ./dns_resolve_stats 1 s " << endl;
    cerr << "          ./dns_resolve_stats 100 m " << endl;
    cerr << "          ./dns_resolve_stats 10 s v" << endl;
}


// Global Variables to print stats at the CTRL-C
static bool dont_stop = true;
mysqlpp::Connection conn;
Server servers[MAX_SERVERS];

void INTHandler(int sig) {
    dont_stop = false;
    cout << endl << endl;
    cout << "### STATS at the END ###" << endl;
    for (int i = 0; i < MAX_SERVERS; i++) {
        servers[i].printServerStats(conn);
    }
}

int
main(int argc, char *argv[])
{
    int i;
    int sleep = 1;
    bool secs = true;
    bool verbose = false;

    if ((argc == 3) || (argc == 4)) {
        sleep = atoi(argv[1]);
        if ((sleep < 1) || (sleep > 1000)) {
            cout << "Invalid <time> " << argv[1] << endl;    
            goto myexit;
        }
        if (argv[2][0] == 's') {
            secs = true;
            sleep = sleep * 1000 * 1000;
        } else if (argv[2][0] == 'm') {
            secs = false;
            sleep = sleep * 1000;
        } else {
            cout << "Invalid <unit> " << argv[2] << endl;
            goto myexit; 
        }
        if (argc == 4) {
            if (argv[3][0] == 'v') {
                verbose = true;
            } else {
                cout << "Invalid <verbose> " << argv[3] << endl;
                goto myexit;
            }
        }
    } else {
myexit:
        usage();
        exit(EXIT_FAILURE);
    }

    // Connect to the database.
    try {
        cout << "Connecting to database server..." << endl;
        conn.connect(0, DB_HOST, DB_USR, DB_PWD);
    }
    catch (exception& er) {
        cerr << "Connection failed: " << er.what() << endl;
        return 1;
    }

    // Create database. If exist just drop current table
    // Suppressing exceptions, because it's not an error if DB doesn't yet exist.
    bool new_db = false;
    {
        mysqlpp::NoExceptions ne(conn);
        mysqlpp::Query query = conn.query();
        if (!conn.select_db(DB_NAME)) {
            // Database doesn't exist yet, so create and select it.
            if (conn.create_db(DB_NAME) &&
                    conn.select_db(DB_NAME)) {
                new_db = true;
            }
            else {
                cerr << "Error creating DB: " << conn.error() << endl;
                return 1;
            }
        }
    }
    if (new_db) {
        cout << "Creating top Servers table..." << endl;
        mysqlpp::Query query = conn.query();
        query << "CREATE TABLE Servers ( " <<
                 "id MEDIUMINT UNSIGNED NOT NULL AUTO_INCREMENT , " <<
                 "name CHAR(50) NOT NULL , " <<
                 "query_time_avg DOUBLE NOT NULL , " <<
                 "query_time_std_dev DOUBLE NOT NULL , " <<
                 "num_of_queries BIGINT UNSIGNED NOT NULL, " <<
                 "first_query TIMESTAMP NULL, " <<
                 "last_query TIMESTAMP NULL, " <<
		         "PRIMARY KEY (id)) ENGINE=INNODB";
        query.execute();
        query << "CREATE TABLE Queries ( " <<
                 "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT , " <<
                 "server_id MEDIUMINT UNSIGNED NOT NULL, " <<
                 "delay_usec BIGINT UNSIGNED NOT NULL, " <<
                 "date TIMESTAMP NOT NULL, " <<
                 "PRIMARY KEY (id), " <<
                 "INDEX (server_id), " <<
                 "FOREIGN KEY (server_id) REFERENCES Servers(id) " <<
                 "ON DELETE CASCADE) ENGINE=INNODB";
        query.execute();
    }

    cout << endl << "### STATS at the Beginning ###" << endl;
    
    for (i = 0; i < MAX_SERVERS; i++) {
        servers[i].init(conn, server_strs[i]);
    }
    
    cout << endl << "# Finish the test with CTRL-C #" << endl;

    signal(SIGINT, INTHandler);
    while(dont_stop) {
        for (i = 0; i < MAX_SERVERS; i++) {
            servers[i].query(conn, verbose);
        }
        usleep(sleep);
    }
    return 0;
}
