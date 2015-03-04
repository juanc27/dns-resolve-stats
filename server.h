/* definition of the server class */
#include <mysql++.h>
#include <sql_types.h>

class Server 
{
    private:
        char name[30];
        mysqlpp::sql_mediumint_unsigned id;
        mysqlpp::sql_double query_time_avg;
        mysqlpp::sql_double query_time_std_dev;
        mysqlpp::sql_bigint_unsigned num_of_queries;
        mysqlpp::sql_timestamp first_query;
        mysqlpp::sql_timestamp last_query;
        bool getServerFromDB(mysqlpp::Connection conn, char name_in[30]);
        void printServerStatsFromRes(mysqlpp::StoreQueryResult res);
        void updateServerStats(mysqlpp::Connection conn, bool verbose);
    public:
        void init(mysqlpp::Connection conn, char name[30]);
        void query(mysqlpp::Connection conn, bool verbose);
        void printServerStats(mysqlpp::Connection conn);
};
