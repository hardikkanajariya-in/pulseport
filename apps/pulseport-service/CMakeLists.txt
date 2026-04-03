add_executable(pulseport-service
    main.cpp
)

target_link_libraries(pulseport-service PRIVATE
    pulseport-core
    pulseport-storage
    pulseport-server
    pulseport-config
    pulseport-diagnostics
    pulseport-metrics
    pulseport-windows
)

# Windows-specific settings
if(WIN32)
    target_link_libraries(pulseport-service PRIVATE
        ws2_32
        advapi32
        shell32
    )
    # Set subsystem to console for debug, windows for release service
    set_target_properties(pulseport-service PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# Install target
install(TARGETS pulseport-service
    RUNTIME DESTINATION bin
)
