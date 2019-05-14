set(SQLPP11_MYSQL_INSTALL_DIR ${CMAKE_THIRD_PARTY_DIR})

set(_byproducts
  ${SQLPP11_MYSQL_INSTALL_DIR}/bin/sqlpp11-ddl2cpp
  ${SQLPP11_MYSQL_INSTALL_DIR}/lib/libsqlpp-mysql.lib
  ${SQLPP11_MYSQL_INSTALL_DIR}/lib/libsqlpp-mysql.a
)

ExternalProject_Add(
  sqlpp11-connector-mysql
  GIT_REPOSITORY https://github.com/rbock/sqlpp11-connector-mysql
  GIT_TAG c3586d373ca73b6f27e574e374ac0e0bce41b680
  CMAKE_ARGS -G ${CMAKE_GENERATOR} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${SQLPP11_MYSQL_INSTALL_DIR} -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH} -DSQLPP11_INCLUDE_DIR=${SQLPP11_INCLUDE_DIR} -DDATE_INCLUDE_DIR=${DATE_INCLUDE_DIR} -DENABLE_TESTS=OFF
  BUILD_BYPRODUCTS ${_byproducts}
  DEPENDS mysql::mysql
  DEPENDS SQLPP11::SQLPP11
  INSTALL_DIR ${SQLPP11_MYSQL_INSTALL_DIR}
  PATCH_COMMAND ${PATCH_SCRIPT_PATH} ${CMAKE_PATCH_DIR}/sqlpp11_mysql.patch
)

ExternalProject_Get_Property(
  sqlpp11-connector-mysql
  install_dir
)

set(SQLPP_CONNECTOR_MYSQL_INCLUDE_DIR "${install_dir}/include")
if(NOT MINGW AND WIN32)
  set(SQLPP11_MYSQL_LIBRARIES "${install_dir}/lib/sqlpp-mysql.lib" "${MYSQL_LIBRARIES}")
else()
  set(SQLPP11_MYSQL_LIBRARIES "${install_dir}/lib/libsqlpp-mysql.a" "${MYSQL_LIBRARIES}")
endif()
if(NOT TARGET SQLPP11::SQLPP11_MYSQL)
    add_library(SQLPP11::SQLPP11_MYSQL INTERFACE IMPORTED)
    add_dependencies(SQLPP11::SQLPP11_MYSQL sqlpp11-connector-mysql)
    set_target_properties(SQLPP11::SQLPP11_MYSQL PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SQLPP_CONNECTOR_MYSQL_INCLUDE_DIR}")
    set_target_properties(SQLPP11::SQLPP11_MYSQL PROPERTIES INTERFACE_LINK_LIBRARIES "${SQLPP11_MYSQL_LIBRARIES}")
endif()