#ifndef _MYSQL_H
#define _MYSQL_H
#include "packdef.h"
#include <mysql/mysql.h>
#include<list>
#include<string>

using namespace  std;


class CMysql{
public:
    int ConnectMysql(const char *server, const char *user, const char *password, const char *database);
    int SelectMysql(char* szSql,int nColumn,list<string>& lst);
    int UpdataMysql(char *szsql);
    int QueryMysql(char* szSql);
    void DisConnect();
public:
    CMysql() : conn(nullptr) { pthread_mutex_init(&m_lock, NULL); }
    ~CMysql() { DisConnect(); pthread_mutex_destroy(&m_lock); }
private:
    MYSQL *conn;

    pthread_mutex_t m_lock;
};




#endif
