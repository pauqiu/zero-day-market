#include "object.h"

Object::Object(std::string name,
                std::string description,
                int lastTransactionDate):
                name(name),
                description(description),
                lastTransactionDate(lastTransactionDate)
{
}

Object::~Object()
{
}