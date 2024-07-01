// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "ortx_common.h"
#include <cuda.h>
#include <cublas_v2.h>

enum AttentionQkvFormat {
  UNKNOWN,               // enum value not set, or depends on qkv projection implementation details
  Q_K_V_BNSH,            // for non-packed qkv, permuted
  Q_K_V_BSNH,            // for non-packed qkv, not permuted, used by memory efficient attention or MultiHeadAttention
  QKV_BSN3H,             // for TRT fused attention, qkv are packed
  Q_K_V_BNSH_QKV_BS3NH,  // for TRT fused causal attention, data has two formats (qkv is 3BNSH, gemm_buffer is BS3NH)
  Q_KV_BSNH_BSN2H,       // for TRT fused cross attention, kv are packed
  Q_K_V_TNH,             // for memory efficient attention, qkv are not packed, and paddings are removed.
  QKV_TN3H,              // for TRT fused attention, qkv are packed and paddings are removed
};

struct PackedAttentionParameters {
  int batch_size;
  int sequence_length;
  int input_hidden_size;  // hidden size of input
  int hidden_size;        // hidden size of Q or K
  int head_size;          // hidden size per head of Q or K
  int v_hidden_size;      // hidden size of V
  int v_head_size;        // hidden size per head of V
  int num_heads;
  int num_kv_heads;
  float scale;
  int token_count;
  int valid_token_count;
  bool has_relative_position_bias;
  bool broadcast_res_pos_bias;
  bool causal;
};

template <typename T>
struct PackedMultiHeadAttentionData {
  const T* query;
  const T* key;
  const T* value;
  const T* bias;
  const T* relative_position_bias;
  const int32_t* token_offset;
  const int32_t* cumulative_sequence_length;

  AttentionQkvFormat source_qkv_format;

  bool no_qkv_workspace;
  T* workspace;
  T* output;

  void* fused_runner;

  bool use_flash_attention;
  bool use_memory_efficient_attention;
};

namespace cuda {
void reshape_and_cache(
    const cudaStream_t stream,
    const void* key,          // [num_tokens, num_heads, head_size]
    const void* value,        // [num_tokens, num_heads, head_size]
    const void* key_cache,    // [num_blocks, block_size, num_heads, head_size]
    const void* value_cache,  // [num_blocks, block_size, num_heads, head_size]
    const int* slot_mapping,  // [num_tokens]
    const int32_t* key_shapes,
    const int32_t* value_shapes,
    const int64_t block_size);
//    void* kv_quant_param = nullptr,  // [num_blocks, 2, num_heads, head_size / kv_quant_chunk_size, block_size]
//    const int kv_quant_chunk_size = 0,
//    const int kv_quant_param_dtype = 1);

void rotary_embedding_neox(
    const cudaStream_t stream,
    const int32_t* positions,  // [num_tokens]
    void* query,               // [num_tokens, num_heads * head_size]
    void* key,                 // [num_tokens, num_kv_heads * head_size]
    int head_size,
    const void* cos_sin_cache,  // [max_position, rot_dim]
    int num_tokens,
    int rot_dim,
    int num_heads,
    int num_kv_heads);

template <typename T>
OrtStatusPtr QkvToContext(
    cudaStream_t stream,
    PackedAttentionParameters& parameters,
    PackedMultiHeadAttentionData<T>& data);

size_t GetAttentionWorkspaceSize(
    size_t element_size,
    size_t batch_size,
    size_t num_heads,
    size_t qk_head_size,
    size_t v_head_size,
    size_t sequence_length,
    void* fused_runner,
    bool use_flash_attention,
    bool use_memory_efficient_attention,
    bool no_qkv_workspace);

} // namespace cuda