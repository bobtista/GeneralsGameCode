set(BUILDVERSIONUPDATE_SRC
    "buildVersionUpdate.cpp"
)

add_executable(z_buildversionupdate WIN32)
set_target_properties(z_buildversionupdate PROPERTIES OUTPUT_NAME buildversionupdate)

target_sources(z_buildversionupdate PRIVATE ${BUILDVERSIONUPDATE_SRC})
