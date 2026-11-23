#ifndef ADEF_H
#define ADEF_H

/**
\mainpage ADEF

The API documentation.

\sa https://github.com/ukjhsa/adef to download the source code.
*/

#include <memory>
#include "System.h"

/// namespace for ADEF
namespace adef {

class PrototypeManager;

/**
@brief Register all built-in ADEF prototype types into @p pm.
*/
void register_type(PrototypeManager& pm);

/**
@brief Initialize the ADEF.
@param argc The argument count.
@param argv The argument vector.
@return The System of ADEF to use.
*/
std::shared_ptr<System> init_adef(int argc, char *argv[]);

}

#endif // ADEF_H
