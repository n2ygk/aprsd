#include <exception>
#include <string>


class TAprsdException: public exception {

private:
    string what_str;

public:
    TAprsdException(const string &what_arg):
        what_str(what_arg) { }
    virtual const char *what() const {
        return what_str.c_str();
    }
};

