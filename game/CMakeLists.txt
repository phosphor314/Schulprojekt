# Add source to this project's executable.
add_executable (WizardThing "renderEngine.h" "renderEngine.cpp" "../tiny_obj_loader.h" "material.h" "material.cpp" "game.h" "game.cpp" "particles.h" "particles.cpp" "constants.h" "BufferStructs.h" "level.h" "level.cpp" "main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET WizardThing PROPERTY CXX_STANDARD 20)
endif()

if (CMAKE_BUILD_TYPE == Release)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -march=native -Wno-ignored-attributes -O3")
endif()

target_link_libraries(WizardThing PUBLIC glm::glm)
target_include_directories(WizardThing PUBLIC ${GLM_INCLUDE_DIRS})
target_link_libraries(WizardThing PUBLIC ${Vulkan_LIBRARIES})
target_include_directories(WizardThing PUBLIC "/home/phosp/vulkan/1.4.335.0/x86_64/include/")
target_link_libraries(WizardThing PUBLIC glfw)
