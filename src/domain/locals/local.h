#ifndef LOCAL_H
#define LOCAL_H

#include <string>

class Local {
    private:
        std::string commercialName;
        std::string domainName;
        std::string websiteName;

    public:
        Local(std::string commercialName,
            std::string domainName,
            std::string websiteName
        );
        
        ~Local();
};

#endif // LOCAL_H