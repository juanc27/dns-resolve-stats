#include <iostream>
#include "server.h"
#include <mysql++.h>
#include <cstring>
#include <ctime>
#include <ldns/ldns.h>
#include <cmath>

using namespace std;

#define MAX_TMP_STR_SIZE 40
#define MAX_RAND_STR_SIZE 8

mysqlpp::DateTime INIT_TIMESTAMP("00-00-00 00:00");

unsigned long long timestamp_diff_usecs(struct timeval *before, struct timeval *after);
void append_random_to_name_server(char *ret, char *name);

void Server::printServerStatsFromRes(mysqlpp::StoreQueryResult res) {
    //trust no one
    if (res && (res.num_rows() > 0)) {
        mysqlpp::sql_double avg = res[0]["query_time_avg"];
        mysqlpp::sql_double stddev = res[0]["query_time_std_dev"];
        cout << "Name: " << setw(13) << res[0]["name"]           << " | ";
        if (avg < 1000) {
            cout << "Avg: "  << fixed << setw(7) << setprecision(3) << avg << "us | ";
        } else {
            avg = avg / 1000;
            cout << "Avg: "  << fixed << setw(7) << setprecision(3) << avg << "ms | ";
        }

        if (stddev < 1000) {
            cout << "StdDev: "  << fixed << setw(7) << setprecision(3) << stddev << "us | ";
        } else {
            stddev = stddev / 1000;
            cout << "StdDev: "  << fixed << setw(7) << setprecision(3) << stddev << "ms | ";
        }
 
        cout << "Queries: "  << setw(4) << res[0]["num_of_queries"] << " | " <<
                "First: "  << setw(7) << res[0]["first_query"] << " | " <<
                "Last: "  << setw(7) << res[0]["last_query"] << " | " << endl;
    }
}

void Server::printServerStats(mysqlpp::Connection conn) {
    mysqlpp::Query query = conn.query();
    query << "SELECT * FROM Servers WHERE " <<
             "id = " << id;
    mysqlpp::StoreQueryResult res = query.store();
    printServerStatsFromRes(res);
}

void Server::updateServerStats(mysqlpp::Connection conn, bool verbose) {
    mysqlpp::Query query = conn.query();
    
    query << "SELECT server_id, AVG(delay_usec) as avg, STDDEV_POP(delay_usec) as stddev, COUNT(*) as cnt " << 
             "FROM Queries WHERE server_id = " << id << " GROUP BY server_id";
    mysqlpp::StoreQueryResult res = query.store();
    if (res && (res.num_rows() > 0)) {
        query_time_avg = res[0]["avg"];
        query_time_std_dev = res[0]["stddev"];
        num_of_queries = res[0]["cnt"];
    }
    query << "UPDATE Servers " <<
             "SET query_time_avg = " << query_time_avg << ", " <<
                 "query_time_std_dev = " << query_time_std_dev << ", " <<
                 "num_of_queries = " << num_of_queries << ", " <<
                 "first_query = " << mysqlpp::quote << first_query << ", " <<
                 "last_query = " << mysqlpp::quote << last_query << " " <<
             "WHERE id = " << id;
    query.execute();
    if (verbose)
        printServerStats(conn);
} 

bool Server::getServerFromDB(mysqlpp::Connection conn, char name_in[30]) {
    mysqlpp::Query query = conn.query();
    query << "SELECT * FROM Servers WHERE " <<
             "name = " << mysqlpp::quote << name_in;
    mysqlpp::StoreQueryResult res = query.store();
    if (res && (res.num_rows() > 0)) {
        printServerStatsFromRes(res);
        strcpy(name, res[0]["name"]);
        id = res[0]["id"];
        query_time_avg = res[0]["query_time_avg"];
        query_time_std_dev = res[0]["query_time_std_dev"];
        num_of_queries = res[0]["num_of_queries"];
        first_query = res[0]["first_query"];
        last_query = res[0]["last_query"];
        return true;
    } else {
        return false;
    }
    return false;
}

void Server::init(mysqlpp::Connection conn, char name_in[30]) {
    if (!getServerFromDB(conn, name_in)) {
        //if it doesn't. add to the db
        mysqlpp::Query query = conn.query();
        query << "INSERT INTO Servers (name, query_time_avg, query_time_std_dev, num_of_queries, first_query, last_query) " <<
                 "Values (" << mysqlpp::quote << name_in << ", 0.0, 0.0, 0, " << mysqlpp::quote << INIT_TIMESTAMP << 
                 ", " << mysqlpp::quote << INIT_TIMESTAMP <<")";
        query.execute();
        if (!getServerFromDB(conn, name_in)) {
            cerr << "Error retrieving resently inserted values " << endl;
        }
    }
}

void Server::query(mysqlpp::Connection conn, bool verbose) {
    ldns_status s;
    ldns_resolver *res; 
    ldns_rdf *rname;
    ldns_rr_list *addr;
    struct timeval before, after;
    static char tmpStr[MAX_TMP_STR_SIZE];
    char tmpName[MAX_TMP_STR_SIZE];
    unsigned long long delay = 0;

    append_random_to_name_server(tmpName, name);
    /* create a new resolver from /etc/resolv.conf */
    s = ldns_resolver_new_frm_file(&res, NULL);
    rname = ldns_dname_new_frm_str(tmpName);
    if (s != LDNS_STATUS_OK) {
        ldns_rdf_deep_free(rname);
        exit(EXIT_FAILURE);
    }
    ldns_resolver_set_retry(res, 1); 

    //GET TIMESTAMP 
    gettimeofday(&before, NULL);
    addr = ldns_get_rr_list_addr_by_name(res, rname, LDNS_RR_CLASS_IN, LDNS_RD);
    //GET TIMESTAMP
    gettimeofday(&after, NULL);

    delay = timestamp_diff_usecs(&before, &after);
    if (verbose) {
        strftime(tmpStr, MAX_TMP_STR_SIZE, "%m/%d/%Y %H:%M:%S", localtime(&before.tv_sec));
        cout << endl << "Qrying " << name << " with: " << tmpName << " start: " << tmpStr << "(" << before.tv_usec << ")";
        strftime(tmpStr, MAX_TMP_STR_SIZE, "%m/%d/%Y %H:%M:%S", localtime(&after.tv_sec));
        cout << " end: " << tmpStr << "(" << after.tv_usec << ")";
        cout << " Delay: " << delay  << "us" << endl;
    }

    mysqlpp::DateTime date(before.tv_sec);
    mysqlpp::Query query = conn.query();
    query << "INSERT INTO Queries (server_id, delay_usec, date) " <<
                 "Values (" << id << ", " << delay << ", " << mysqlpp::quote << date << ")";
    query.execute();

    if (first_query == INIT_TIMESTAMP) {
        first_query = date;
    }
    last_query = date;
    updateServerStats(conn, verbose);

    ldns_rdf_deep_free(rname);
    ldns_resolver_deep_free(res);
}

unsigned long long timestamp_diff_usecs(struct timeval *before, struct timeval *after) {
    long long before_usecs = before->tv_sec*1000000LL + before->tv_usec;
    long long after_usecs = after->tv_sec*1000000LL + after->tv_usec; 
    return after_usecs - before_usecs;
}

// Taken from stackoverflow.com, user: Ates Goral
// http://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
void gen_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

void append_random_to_name_server(char *ret, char *name) {
    gen_random(ret, MAX_RAND_STR_SIZE);
    ret[MAX_RAND_STR_SIZE] = '.'; 
    strcpy(&ret[MAX_RAND_STR_SIZE + 1], name);
}
