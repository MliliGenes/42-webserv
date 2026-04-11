#include "../include/TrpValidatorContext.hpp"

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
    std::string full_path;

    // Pre-calculate total size needed
    size_t total_size = 0;
    for (size_t i = 0; i < paths.size(); ++i) {
        total_size += paths[i].length();
    }
    
    // Reserve space upfront to avoid reallocations during append
    full_path.reserve(total_size);
    
    // Use index-based iteration to avoid iterator invalidation
    for (size_t i = 0; i < paths.size(); ++i) {
        full_path += paths[i];
    }

    return full_path;
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