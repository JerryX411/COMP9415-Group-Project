#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

TEST_CASE("Translation matrices move points by the requested offset",
          "[math]") {
  const glm::mat4 translation =
      glm::translate(glm::mat4(1.0F), glm::vec3(2.0F, -3.0F, 4.0F));

  // A w value of 1 makes the translation part of the matrix affect the point.
  const glm::vec4 point{1.0F, 2.0F, 3.0F, 1.0F};
  const glm::vec4 moved = translation * point;

  REQUIRE(moved.x == 3.0F);
  REQUIRE(moved.y == -1.0F);
  REQUIRE(moved.z == 7.0F);
  REQUIRE(moved.w == 1.0F);
}
