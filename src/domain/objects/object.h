#ifndef OBJECT_H
#define OBJECT_H

#include <string>

class Object {
    private:
        int objectID;
        std::string name;
        std::string description;
        int lastTransactionDate; // TODO: Change this to a proper type later

    public:
        Object(std::string name,
                std::string description,
                int lastTransactionDate
            );
            
        ~Object();
};

#endif // OBJECT_H