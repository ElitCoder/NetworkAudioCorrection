#ifndef BASE_H
#define BASE_H

#include "System.h"

class Base {
public:
	static System& system();
	
private:
	static System system_;
};

#endif