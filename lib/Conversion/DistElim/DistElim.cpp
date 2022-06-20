//===- DistElim.h - PTensorToLinalg conversion  ---------*- C++ -*-===//
//
// Copyright 2022 Intel Corporation
// Part of the IMEX Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements elimination of the Dist dialects, leading to local-only
/// operation
///
//===----------------------------------------------------------------------===//

#include <imex/Conversion/DistElim/DistElim.h>
#include <imex/Dialect/Dist/IR/DistOps.h>
#include <imex/internal/PassWrapper.h>

#include <mlir/Dialect/Arithmetic/IR/Arithmetic.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/Shape/IR/Shape.h>
#include <mlir/Dialect/Tensor/IR/Tensor.h>
#include <mlir/IR/BuiltinOps.h>

// dummy: constant op
template <typename Op>
static void _toConst(Op op, mlir::PatternRewriter &rewriter, int64_t v = 0) {
  auto attr = rewriter.getIndexAttr(v);
  rewriter.replaceOpWithNewOp<mlir::arith::ConstantOp>(op, attr);
}

// *******************************
// ***** Individual patterns *****
// *******************************

// RegisterPTensorOp -> no-op
struct ElimRegisterPTensorOp
    : public mlir::OpRewritePattern<::dist::RegisterPTensorOp> {
  using OpRewritePattern::OpRewritePattern;

  ::mlir::LogicalResult
  matchAndRewrite(::dist::RegisterPTensorOp op,
                  mlir::PatternRewriter &rewriter) const override {
    _toConst(op, rewriter);
    return ::mlir::success();
  }
};

// LocalOffsetsOp -> const(0)
struct ElimLocalOffsetsOp
    : public mlir::OpRewritePattern<::dist::LocalOffsetsOp> {
  using OpRewritePattern::OpRewritePattern;

  ::mlir::LogicalResult
  matchAndRewrite(::dist::LocalOffsetsOp op,
                  mlir::PatternRewriter &rewriter) const override {
    _toConst(op, rewriter);
    return ::mlir::success();
  }
};

// LocalShapeOp -> global shape
struct ElimLocalShapeOp : public mlir::OpRewritePattern<::dist::LocalShapeOp> {
  using OpRewritePattern::OpRewritePattern;

  ::mlir::LogicalResult
  matchAndRewrite(::dist::LocalShapeOp op,
                  mlir::PatternRewriter &rewriter) const override {
    auto x = op.ptensor().getDefiningOp<::dist::RegisterPTensorOp>();
    assert(x);
    x.shape().dump();
    rewriter.replaceOp(op, x.shape());
    return ::mlir::success();
  }
};

// AllReduceOp -> identity cast
struct ElimAllReduceOp : public mlir::OpRewritePattern<::dist::AllReduceOp> {
  using OpRewritePattern::OpRewritePattern;

  ::mlir::LogicalResult
  matchAndRewrite(::dist::AllReduceOp op,
                  mlir::PatternRewriter &rewriter) const override {
#if 0
    auto loc = op.getLoc();

    ::mlir::ModuleOp module = op->getParentOfType<::mlir::ModuleOp>();
    auto *context = module.getContext();
    constexpr auto _f = "printf";
    if(!module.lookupSymbol<::mlir::func::FuncOp>(_f)) {
        // auto st = rewriter.getStringAttr("dummy");
        // auto fn = ::mlir::FlatSymbolRefAttr::get(st);
        auto fn = ::llvm::StringRef(_f);
        auto ft = rewriter.getFunctionType({}, {});
        // Insert the printf function into the body of the parent module.
        ::mlir::PatternRewriter::InsertionGuard insertGuard(rewriter);
        rewriter.setInsertionPointToStart(module.getBody());
        rewriter.create<::mlir::func::FuncOp>(loc, _f, ft);
    }
    auto fa = ::mlir::SymbolRefAttr::get(context, _f);
    auto fc = rewriter.create<::mlir::func::CallOp>(loc, fa, ::mlir::TypeRange{});
#endif
    rewriter.replaceOpWithNewOp<::mlir::tensor::CastOp>(
        op, op.tensor().getType(), op.tensor());
    return ::mlir::success();
  }
};

// *******************************
// ***** Pass infrastructure *****
// *******************************

namespace imex {

/// Populate the given list with patterns that eliminate Dist ops
void populateDistElimConversionPatterns(::mlir::LLVMTypeConverter &converter,
                                        ::mlir::RewritePatternSet &patterns);

// Lowering dist dialect by no-ops
struct DistElimPass : public ::imex::PassWrapper<
                          DistElimPass, ::mlir::ModuleOp,
                          ::imex::DialectList<::mlir::shape::ShapeDialect>,
                          ElimRegisterPTensorOp, ElimLocalOffsetsOp,
                          ElimLocalShapeOp, ElimAllReduceOp> {};

/// Create a pass to eliminate Dist ops
std::unique_ptr<::mlir::OperationPass<::mlir::ModuleOp>>
createConvertDistElimPass() {
  return std::make_unique<DistElimPass>();
}

} // namespace imex