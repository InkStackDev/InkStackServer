#pragma once

class DBManager {
public:
    static bool CreateAllTables();

    static bool MigrateAllTables();
};
