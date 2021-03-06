/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include <cl_common.h>

__kernel void elementwise_add(__read_only image2d_t input, __read_only image2d_t bias, __write_only image2d_t outputImage) {
     int x = get_global_id(0);
     int y = get_global_id(1);
     const sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
     int2 coords;
     coords.x = x;
     coords.y = y;
     float4 in = read_imagef(input, sampler, coords);
     float4 biase = read_imagef(bias, sampler, coords);
     float4 output = in + biase;
     write_imagef(outputImage,coords,output);
 }
