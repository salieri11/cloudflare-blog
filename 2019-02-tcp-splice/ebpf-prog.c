/* AUTOGENERATED DO NOT EDIT */
#include <linux/bpf.h>
#include <stddef.h>
#include <stdint.h>

#include "tbpf.h"

size_t bpf_insn_prog_parser_cnt = 2;

struct bpf_insn bpf_insn_prog_parser[] = {{
						  .code = 0x61,
						  .dst_reg = BPF_REG_0,
						  .src_reg = BPF_REG_1,
						  .off = 0,
						  .imm = 0 /**/
					  },
					  {
						  .code = 0x95,
						  .dst_reg = BPF_REG_0,
						  .src_reg = BPF_REG_0,
						  .off = 0,
						  .imm = 0 /**/
					  }};

struct tbpf_reloc bpf_reloc_prog_parser[] = {
	{.name = NULL, .type = 0, .offset = 0}};

size_t bpf_insn_prog_verdict_cnt = 6;

struct bpf_insn bpf_insn_prog_verdict[] = {
	{
		.code = 0x18,
		.dst_reg = BPF_REG_2,
		.src_reg = BPF_REG_0,
		.off = 0,
		.imm = 0 /* relocation for sock_map */
	},
	{
		.code = 0x0,
		.dst_reg = BPF_REG_0,
		.src_reg = BPF_REG_0,
		.off = 0,
		.imm = 0 /**/
	},
	{
		.code = 0xb7,
		.dst_reg = BPF_REG_3,
		.src_reg = BPF_REG_0,
		.off = 0,
		.imm = 0 /**/
	},
	{
		.code = 0xb7,
		.dst_reg = BPF_REG_4,
		.src_reg = BPF_REG_0,
		.off = 0,
		.imm = 0 /**/
	},
	{
		.code = 0x85,
		.dst_reg = BPF_REG_0,
		.src_reg = BPF_REG_0,
		.off = 0,
		.imm = 52 /**/
	},
	{
		.code = 0x95,
		.dst_reg = BPF_REG_0,
		.src_reg = BPF_REG_0,
		.off = 0,
		.imm = 0 /**/
	}};

struct tbpf_reloc bpf_reloc_prog_verdict[] = {
	{.name = "sock_map", .type = 1, .offset = 0},
	{.name = NULL, .type = 0, .offset = 0}};
