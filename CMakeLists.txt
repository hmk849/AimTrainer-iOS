cmake_minimum_required(VERSION 3.24)

project(AimTrainer LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.12/SDL2-2.30.12.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(SDL2_ttf
  URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL2_ttf)

add_executable(AimTrainer MACOSX_BUNDLE sb.cpp)

target_link_libraries(AimTrainer PRIVATE
  SDL2::SDL2-static
  SDL2_ttf::SDL2_ttf-static
)

if(TARGET SDL2::SDL2main)
  target_link_libraries(AimTrainer PRIVATE SDL2::SDL2main)
endif()

set_target_properties(AimTrainer PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "AimTrainer"
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.hmk849.aimtrainer"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.hmk849.aimtrainer"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
)
