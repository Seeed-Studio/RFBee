#ifndef DEBUG_H
#define DEBUG_H 1

#ifdef DEBUG
    #define DEBUGPRINT( X ) \
    Serial.print( __FUNCTION__ ); \
    Serial.print( ": " ); \
    Serial.println( X );
#else
    #define DEBUGPRINT( X )
#endif
#endif

