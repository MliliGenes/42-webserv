#include "../include/TrpValidatorContext.hpp"
#include <sstream>

TrpValidatorContext::TrpValidatorContext( void ) {}

void TrpValidatorContext::pushPath( const std::string _path ) {
    if ( _path.empty() ) return;
    paths.push_back( _path );
}

void TrpValidatorContext::popPath( void ) {
    if ( paths.empty() ) return;
    paths.pop_back();
}

void TrpValidatorContext::pushError( ValidationError err ) {
    errors.push_back( err );
}

std::string TrpValidatorContext::getCurrentPath( void ) {
    std::ostringstream oss;
    
    // Defensive approach: use stringstream with bounds checks
    const size_t path_count = paths.size();
    for (size_t i = 0; i < path_count; ++i) {
        if (i < paths.size()) {
            oss << paths[i];
        }
    }

    return oss.str();
}

const TrpValidationError& TrpValidatorContext::getErrors( void ) const {
    return errors;
}

bool TrpValidatorContext::printErrors( void ) const {
    bool got_errors = !errors.empty();
    for (size_t i = 0; i < errors.size(); i++) {
        std::cerr
            << errors[i].path
            << ": "
            << errors[i].msg
            << std::endl;
    }
    return got_errors;
}