cmake_minimum_required(VERSION 2.8)

add_executable(prime-sieve EXCLUDE_FROM_ALL prime-sieve.cc)
target_link_libraries(prime-sieve ten)

add_executable(echo-server EXCLUDE_FROM_ALL echo-server.cc)
target_link_libraries(echo-server ten)

add_executable(chat-server EXCLUDE_FROM_ALL chat-server.cc)
target_link_libraries(chat-server ten)

add_executable(stack-guard EXCLUDE_FROM_ALL stack-guard.cc)
target_link_libraries(stack-guard ten)

add_executable(stack-trace EXCLUDE_FROM_ALL stack-trace.cc)
target_link_libraries(stack-trace ten)

add_executable(http-get EXCLUDE_FROM_ALL http-get.cc)
target_link_libraries(http-get ten)

add_executable(http-proxy EXCLUDE_FROM_ALL http-proxy.cc)
target_link_libraries(http-proxy ten)

add_executable(http-server EXCLUDE_FROM_ALL http-server.cc)
target_link_libraries(http-server ten boost_program_options)

add_executable(msg-pack EXCLUDE_FROM_ALL msgpack.cc)
target_link_libraries(msg-pack ten jansson msgpack)

add_executable(rpc-server EXCLUDE_FROM_ALL rpc-server.cc)
target_link_libraries(rpc-server ten msgpack)

add_executable(json-path EXCLUDE_FROM_ALL json-path.cc)
target_link_libraries(json-path ten jansson msgpack boost_program_options)

add_executable(memg EXCLUDE_FROM_ALL memg.cc)
target_link_libraries(memg ten boost_program_options)

add_executable(synch EXCLUDE_FROM_ALL synch.cc)
target_link_libraries(synch ten)

if (OPENSSL_FOUND)
    add_executable(https-get EXCLUDE_FROM_ALL https-get.cc)
    target_link_libraries(https-get ten)
    set(HTTPS_GET https-get)
endif (OPENSSL_FOUND)

add_custom_target(examples DEPENDS
    prime-sieve
    echo-server
    chat-server
    stack-guard
    stack-trace
    http-get
    http-proxy
    http-server
    msg-pack
    rpc-server
    json-path
    memg
    synch
    ${HTTPS_GET}
    )
