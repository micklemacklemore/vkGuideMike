# To Do List!

Up to: https://vkguide.dev/docs/chapter-4/double_buffering/

https://vulkan-tutorial.com/Texture_mapping/Images

Barriers are primarily used for synchronization purposes, so you must specify which types of operations that involve the resource must happen before the barrier, and which operations that involve the resource must wait on the barrier. We need to do that despite already using vkQueueWaitIdle to manually synchronize. The right values depend on the old and new layout, so we'll get back to this once we've figured out which transitions we're going to use.


## Questions: 

* what is the purpose of a `VkVertexInputBindingDescription` and a `VkVertexInputAttributeDescription`? 

# Done! 

* magic ball interface for rotating the camera ? https://www.youtube.com/watch?v=mHVwd8gYLnI (trackball)
  + https://raw.org/code/trackball-rotation-using-quaternions/