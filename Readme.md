# To Do List!

Up to: https://vkguide.dev/docs/chapter-3/scene_management/ 

* Create multiple pipelines with newer shaders, and use them to render monkeys each with a different material each
* Load more meshes. As long as it’s an obj with TRIANGLE meshes, it should work fine. Make sure on export that the obj includes normals and colors
* Add WASD controls to the camera. For that, you would need to modify the camera matrices in the draw functions.
* Sort the renderables array before rendering by Pipeline and Mesh, to reduce number of binds.

* magic ball interface for rotating the camera ? https://www.youtube.com/watch?v=mHVwd8gYLnI (trackball)
* https://raw.org/code/trackball-rotation-using-quaternions/

## Questions: 

* what is the purpose of a `VkVertexInputBindingDescription` and a `VkVertexInputAttributeDescription`? 