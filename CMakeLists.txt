cmake_minimum_required( VERSION 3.16.0 )

idf_component_register(
    SRCS "unit_pahub.c"
    INCLUDE_DIRS "./include"
    REQUIRES "Core2-for-AWS-IoT-Kit"
)