# To Do List!

Up to: https://vkguide.dev/docs/chapter-4/double_buffering/

* Create multiple pipelines with newer shaders, and use them to render monkeys each with a different material each
* Load more meshes. As long as it’s an obj with TRIANGLE meshes, it should work fine. Make sure on export that the obj includes normals and colors
* Add WASD controls to the camera. For that, you would need to modify the camera matrices in the draw functions.
* Sort the renderables array before rendering by Pipeline and Mesh, to reduce number of binds.

## Questions: 

* what is the purpose of a `VkVertexInputBindingDescription` and a `VkVertexInputAttributeDescription`? 

# Done! 

* magic ball interface for rotating the camera ? https://www.youtube.com/watch?v=mHVwd8gYLnI (trackball)
  + https://raw.org/code/trackball-rotation-using-quaternions/