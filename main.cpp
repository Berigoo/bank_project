#ifdef WINDOWS
#define CLEAR "cls"
#else
#define CLEAR "clear"
#endif

#include <iostream>
#include "sstream"
#include "ctime"
#include "cmath"
#include "mariadb/conncpp.hpp"

using namespace std;

string toReadable(string num){
    int decimalPoint = num.length() - num.find('.');
    int len = num.length()-decimalPoint;
    for(int i=0; i<(int)(len-1)/3; i++){
        num.insert(num.length()-decimalPoint-((i+1)*3)-i, ".");
    }
    return num;
}
struct dbConn{
private:
    sql::SQLString url;
    sql::Driver* driver;
public:
    string host, db;
    sql::SQLString user, passwd;
    unique_ptr<sql::Connection> connection;

    dbConn(string host, string user, string passwd, string db) {
        this->host = host;
        this->user = user;
        this->passwd = passwd;
        this->db = db;

        url = "jdbc:mariadb://" + host + ":3306/" + db;
        driver = sql::mariadb::get_driver_instance();
    }

    void connInit(){
        connection = unique_ptr<sql::Connection>(driver->connect(url, user, passwd));
        if(!connection){
            cerr << "Invalid database connection";
            exit(EXIT_FAILURE);
        }
    }

    sql::ResultSet* execPreparedQuery(string statement, vector<string> values){
        shared_ptr<sql::PreparedStatement> stmt(connection->prepareStatement(statement));
        try {
            for (int i = 1; i <= values.size(); i++) stmt->setString(i, values[i-1]);
            sql::ResultSet* tmp(stmt->executeQuery());
            return tmp;
        }catch(sql::SQLException &e) {
            cerr << "Error reading database: " <<
                    e.what() << endl;
        }
        return nullptr;
    }
};
struct userHistory{
    bool isCommited = false;
    int sender_id, recipient_id;
    string recipient_account, recipient_name, dateTime="-";
    float amount;

    userHistory* next = nullptr;

    void commit(dbConn* conn){
        time_t t = time(0);
        tm* now = localtime(&t);
        string dateNow = to_string(now->tm_year+1900) + "-" + to_string(now->tm_mon+1) + "-" + to_string(now->tm_mday)
                + " " + to_string(now->tm_hour) + ":" + to_string(now->tm_min) + ":" + to_string(now->tm_sec);
        conn->execPreparedQuery("INSERT INTO user_history VALUES(NULL, ?, ?, ?, ?)", {to_string(sender_id),
                                                                                                      to_string(recipient_id),
                                                                                                      to_string(amount),
                                                                                                      dateNow});
        isCommited = true;

        sql::ResultSet* res = conn->execPreparedQuery("SELECT * FROM user WHERE id=?", {to_string(recipient_id)});
        if(res->next()) {
            recipient_account = res->getString(5);
            recipient_name = res->getString(2);
            dateTime = dateNow;
        }
    }
    void print(){
        cout << dateTime << "\t" << recipient_account << "\t\t" << recipient_name << "\t\t" << "Rp. " + toReadable(
                to_string(amount)) << '\n';
    }
};
struct user{
public:
    int id;
    string name, account;
    float balance;

    user(dbConn* conn, string& username, string& password){
     sql::ResultSet* res = conn->execPreparedQuery("SELECT * FROM user WHERE username= ? AND password= ? ", {username, password});
     if(res->next()){
         id = res->getInt(1);
         name = res->getString(2);
         balance = res->getFloat(4);
         account = res->getString(5);
     }
    }

    userHistory* history = nullptr;
    static int historySize;

    void addHistory(int sender_id, int recipient_id, float amount) {
        if (!history) {
            history = new userHistory();
            history->sender_id = sender_id;
            history->recipient_id = recipient_id;
            history->amount = amount;
        } else {
            userHistory* aux = history;
            while (aux->next) aux = aux->next;
            userHistory* newNode = new userHistory();
            newNode->sender_id = sender_id;
            newNode->recipient_id = recipient_id;
            newNode->amount = amount;
            aux->next = newNode;
        }
        user::historySize++;
    }
    void addHistory(string date, string recipient_account, string recipient_name, string amount) {
        if (!history) {
            history = new userHistory();
            history->recipient_account = recipient_account;
            history->recipient_name = recipient_name;
            history->amount = stof(amount);
            history->dateTime = date;
            history->isCommited = true;
        } else {
            userHistory *aux = history;
            while (aux->next) aux = aux->next;
            userHistory *newNode = new userHistory();
            newNode->recipient_account = recipient_account;
            newNode->recipient_name = recipient_name;
            newNode->amount = stof(amount);
            newNode->dateTime = date;
            newNode->isCommited = true;
            aux->next = newNode;
        }
        user::historySize++;
        cout << user::historySize;
    }
    void commitUncommited(dbConn* conn){
        if(history) {
            userHistory *aux = history;
            while (aux){
                if(!aux->isCommited) aux->commit(conn);
                aux = aux->next;
            }
        }
    }
    void printHistory(){
        int count = 0;
        cout << "  Waktu\t\t\t   Rekening\t\t   Nama\t\t   Nominal\n";
        if(history) {
            userHistory *aux = history;
            while (aux){
                aux->print();
                aux = aux->next;
                count++;
            }
        }
        cout << "\n\nTotal Entries: " << count << "\nTekan untuk lanjut..."; char dummy;cin >> dummy;
    }
    void dumpHistory(dbConn* conn){
        sql::ResultSet* res = conn->execPreparedQuery("SELECT user.username, user.account_number, tmp.* FROM (SELECT * FROM user_history WHERE sender_id= ? UNION SELECT * FROM user_history WHERE recipient_id= ?) AS tmp INNER JOIN user ON tmp.recipient_id = user.id ORDER BY ts", {
                to_string(id), to_string(id)});
        int count = 0;
        cout << count << " " << user::historySize;
        while(res->next()){
            count++;
            if(count > user::historySize)
                addHistory((string) res->getString(7), (string) res->getString(2), (string) res->getString(1),
                           (string) res->getString(6));
        }
    }
};

int user::historySize;

int main() {
    dbConn conn("localhost", "guest", "p@ssword", "bank_project");
    conn.connInit();
    string username, passwd;
    bool isGranted = true;
    do{
        system(CLEAR);
        if(!isGranted) cout << "\tCredentials Salah!\n";
        cout << "\t\tHalaman Login\n" <<
                "\t username: "; cin >> username;
        cout << "\t password: "; cin >> passwd;
        sql::ResultSet* res = conn.execPreparedQuery("SELECT COUNT(1) FROM user WHERE username= ? AND password= ?", {username, passwd});
        if(res->next()) isGranted = res->getInt(1);
    }while(!isGranted);
    user _user(&conn, username, passwd);
    user::historySize = 0;
    do {
        _user.dumpHistory(&conn);
        int pilihan = -1;
        do {
            system(CLEAR);
            cout << "Welcome, " << _user.name << '\n' <<
                 "\t\tMenu Pilihan\n" <<
                 "\t1. Transfer\n" <<
                 "\t2. Cek Saldo\n" <<
                 "\t3. Cek History\n" <<
                 "\t4. Exit\n" <<
                 "Pilihan: ";
            cin >> pilihan;
        } while (pilihan < 1 || pilihan > 4);

        switch (pilihan) {
            case 1: {
                int feed;
                do {
                    string accountNum;
                    system(CLEAR);
                    cout << "\t\tTransfer\n" <<
                         "Silakan masukan nomor rekening yanag dituju:\n";
                    cin >> accountNum;
                    sql::ResultSet *res = conn.execPreparedQuery("SELECT * FROM user WHERE account_number= ?",
                                                                 {accountNum});
                    if (res->next()) {
                        float amount;
                        do {
                            cout << "Masukan nominal: (IDR 1 - IDR " << _user.balance << " ): ";
                            cin >> amount;
                        } while (amount > _user.balance || amount < 1);

                        system(CLEAR);
                        cout << "\t\tPreview\n" <<
                             "\tNomor Rekening Tujuan: \t\t" << accountNum << '\n' <<
                             "\tNama Rekening Tujuan: \t\t" << res->getString(2) << '\n' <<
                             "\tNomor Rekening Pengirim: \t" << _user.account << '\n' <<
                             "\tNama Pengirim: \t\t\t" << _user.name << '\n' <<
                             "\tNominal: \t\t\tIDR. " << amount << '\n' <<
                             "1. Masukan Ulang Rekening\n" <<
                             "2. OK\n";
                        cin >> feed;
                        if (feed == 2) {
                            _user.balance -= amount;
                            conn.execPreparedQuery("UPDATE user SET balance= ? WHERE id=?", {to_string(_user.balance),
                                                                                             to_string(_user.id)});
                            conn.execPreparedQuery(
                                    "UPDATE user SET balance=(SELECT balance FROM user WHERE account_number= ?) + ? WHERE account_number= ?",
                                    {accountNum,
                                     to_string(amount), accountNum});

                            time_t t = time(0);
                            tm *now = localtime(&t);
                            cout << "\t\tTransfer SUKSES\n" <<
                                 "\tNomor Rekening Tujuan: \t\t" << accountNum << '\n' <<
                                 "\tNama Rekening Tujuan: \t\t" << res->getString(2) << '\n' <<
                                 "\tTanggal transaksi: \t\t" << now->tm_mday << '-' << now->tm_mon + 1 << '-'
                                 << now->tm_year + 1900 << '\n' <<
                                 "\tWaktu transaksi: \t\t" << now->tm_hour << ':' << now->tm_min << ':' << now->tm_sec
                                 << " WIB" << '\n' <<
                                 "\tNomor Rekening Pengirim: \t" << _user.account << '\n' <<
                                 "\tNama Pengirim: \t\t\t" << _user.name << '\n' <<
                                 "\tNominal: \t\t\tIDR. " << amount << '\n' <<
                                 "Tekan untuk lanjut...";
                            char dummy;
                            cin >> dummy;

                            //history commit
                            _user.addHistory(_user.id, res->getInt(1), amount);
                            _user.commitUncommited(&conn);

                        }
                    } else {
                        system(CLEAR);
                        cout << "\tNomor rekening tidak ditemukan!\n" <<
                             "\t1. Masukan ulang\n" <<
                             "\t2. Kembali ke menu\n";
                        cin >> feed;
                    }
                } while (feed == 1);
                break;
            }
            case 2: {
                sql::ResultSet *res = conn.execPreparedQuery("SELECT balance FROM user WHERE id= ?",
                                                             {to_string(_user.id)});
                if(res->next()) {
                    cout << "\t\tInformasi Saldo\n" <<
                         "\tSaldo Anda: Rp. " << toReadable((string)res->getString(1)) << '\n' <<
                         "Tekan untuk lanjut...";
                    char dummy;cin >> dummy;
                }else{
                    cout << "Terjadi kesalahan internal!";
                    char dummy;cin >> dummy;
                }
                break;
            }
            case 3:
                system(CLEAR);
                cout << "\t\tInformasi History\n\n";
                _user.printHistory();
                break;
            case 4:
                conn.connection->close();
                exit(EXIT_SUCCESS);
                break;
        }
    } while (1);
}
