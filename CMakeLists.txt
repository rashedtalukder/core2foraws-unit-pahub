cmake_minimum_required( VERSION 3.16.0 )

set( CORE2FORAWS_LIB            "${COMPONENT_DIR}/../Core2-for-AWS-IoT-Kit/lib" )

set( COMPONENT_SRCDIRS .        "${CORE2FORAWS_LIB}/common/lib/esp-idf-lib/components/tca9548/" )
set( COMPONENT_ADD_INCLUDEDIRS  "./include" 
                                "${CORE2FORAWS_LIB}/common/lib/esp-idf-lib/components/tca9548/" 
                                )
set( COMPONENT_REQUIRES         "Core2-for-AWS-IoT-Kit" )

register_component()