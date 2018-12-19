//===- AffineStructures.h - MLIR Affine Structures Class --------*- C++ -*-===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================
//
// Structures for affine/polyhedral analysis of ML functions.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_ANALYSIS_AFFINE_STRUCTURES_H
#define MLIR_ANALYSIS_AFFINE_STRUCTURES_H

#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/Operation.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {

class AffineApplyOp;
class AffineBound;
class AffineCondition;
class AffineMap;
class ForStmt;
class IntegerSet;
class MLIRContext;
class MLValue;
class HyperRectangularSet;

/// A mutable affine map. Its affine expressions are however unique.
struct MutableAffineMap {
public:
  MutableAffineMap() {}
  MutableAffineMap(AffineMap map);

  ArrayRef<AffineExpr> getResults() const { return results; }
  AffineExpr getResult(unsigned idx) const { return results[idx]; }
  void setResult(unsigned idx, AffineExpr result) { results[idx] = result; }
  unsigned getNumResults() const { return results.size(); }
  unsigned getNumDims() const { return numDims; }
  void setNumDims(unsigned d) { numDims = d; }
  unsigned getNumSymbols() const { return numSymbols; }
  void setNumSymbols(unsigned d) { numSymbols = d; }
  MLIRContext *getContext() const { return context; }

  /// Returns true if the idx'th result expression is a multiple of factor.
  bool isMultipleOf(unsigned idx, int64_t factor) const;

  /// Resets this MutableAffineMap with 'map'.
  void reset(AffineMap map);

  /// Simplify the (result) expressions in this map using analysis (used by
  //-simplify-affine-expr pass).
  void simplify();
  /// Get the AffineMap corresponding to this MutableAffineMap. Note that an
  /// AffineMap will be uniqued and stored in context, while a mutable one
  /// isn't.
  AffineMap getAffineMap() const;

private:
  // Same meaning as AffineMap's fields.
  SmallVector<AffineExpr, 8> results;
  SmallVector<AffineExpr, 8> rangeSizes;
  unsigned numDims;
  unsigned numSymbols;
  /// A pointer to the IR's context to store all newly created
  /// AffineExprStorage's.
  MLIRContext *context;
};

/// A mutable integer set. Its affine expressions are however unique.
struct MutableIntegerSet {
public:
  MutableIntegerSet(IntegerSet set, MLIRContext *context);

  /// Create a universal set (no constraints).
  MutableIntegerSet(unsigned numDims, unsigned numSymbols,
                    MLIRContext *context);

  unsigned getNumDims() const { return numDims; }
  unsigned getNumSymbols() const { return numSymbols; }
  unsigned getNumConstraints() const { return constraints.size(); }

  void clear() {
    constraints.clear();
    eqFlags.clear();
  }

private:
  unsigned numDims;
  unsigned numSymbols;

  SmallVector<AffineExpr, 8> constraints;
  SmallVector<bool, 8> eqFlags;
  /// A pointer to the IR's context to store all newly created
  /// AffineExprStorage's.
  MLIRContext *context;
};

/// An AffineValueMap is an affine map plus its ML value operands and
/// results for analysis purposes. The structure is still a tree form that is
/// same as that of an affine map or an AffineApplyOp. However, its operands,
/// results, and its map can themselves change  as a result of
/// substitutions, simplifications, and other analysis.
// An affine value map can readily be constructed from an AffineApplyOp, or an
// AffineBound of a ForStmt. It can be further transformed, substituted into,
// or simplified. Unlike AffineMap's, AffineValueMap's are created and destroyed
// during analysis. Only the AffineMap expressions that are pointed by them are
// unique'd.
// An affine value map, and the operations on it, maintain the invariant that
// operands are always positionally aligned with the AffineDimExpr and
// AffineSymbolExpr in the underlying AffineMap.
// TODO(bondhugula): Some of these classes could go into separate files.
class AffineValueMap {
public:
  // Creates an empty AffineValueMap (users should call 'reset' to reset map
  // and operands).
  AffineValueMap() {}
  AffineValueMap(const AffineApplyOp &op);
  AffineValueMap(const AffineBound &bound);
  AffineValueMap(AffineMap map);
  AffineValueMap(AffineMap map, ArrayRef<MLValue *> operands);

  ~AffineValueMap();

  /// Substitute the results of inputMap into the operands of this map.
  // The new list of operands will be a union of this map's and that of the map
  // we are substituting from.
  // Example usage scenario: a subscript operand for a 'load' is forward
  // substituted into the memref's access map. The subscript operand itself is
  // then substituted by its defining affine_apply op instructions and
  // successively by a loop IV remap expression, eventually resulting in an
  // affine value map that has only the loop IVs and symbols as its operands.
  // Hence, the access pattern can then be analyzed for example.
  // TODO(bondhugula)
  void forwardSubstitute(const AffineValueMap &inputMap);
  void forwardSubstitute(const AffineApplyOp &inputOp);
  void forwardSubstituteSingle(const AffineApplyOp &inputOp,
                               unsigned inputResultIndex);
  // TODO(andydavis, bondhugula) Expose an affine map simplify function, which
  // can be used to amortize the cost of simplification over multiple fwd
  // substitutions).

  // Resets this AffineValueMap with 'map' and 'operands'.
  void reset(AffineMap map, ArrayRef<MLValue *> operands);
  /// Return true if the idx^th result can be proved to be a multiple of
  /// 'factor', false otherwise.
  inline bool isMultipleOf(unsigned idx, int64_t factor) const;

  /// Return true if the idx^th result depends on 'value', false otherwise.
  bool isFunctionOf(unsigned idx, MLValue *value) const;

  /// Return true if the result at 'idx' is a constant, false
  /// otherwise.
  bool isConstant(unsigned idx) const;

  /// Return true if this is an identity map.
  bool isIdentity() const;

  inline unsigned getNumOperands() const { return operands.size(); }
  inline unsigned getNumDims() const { return map.getNumDims(); }
  inline unsigned getNumSymbols() const { return map.getNumSymbols(); }
  inline unsigned getNumResults() const { return map.getNumResults(); }

  SSAValue *getOperand(unsigned i) const;
  ArrayRef<MLValue *> getOperands() const;
  AffineMap getAffineMap() const;

private:
  void forwardSubstitute(const AffineApplyOp &inputOp,
                         ArrayRef<bool> inputResultsToSubstitute);
  // A mutable affine map.
  MutableAffineMap map;

  // TODO: make these trailing objects?
  /// The SSA operands binding to the dim's and symbols of 'map'.
  SmallVector<MLValue *, 4> operands;
  /// The SSA results binding to the results of 'map'.
  SmallVector<MLValue *, 4> results;
};

/// An IntegerValueSet is an integer set plus its operands.
// Both, the integer set being pointed to and the operands can change during
// analysis, simplification, and transformation.
class IntegerValueSet {
  // Constructs an integer value set map from an IntegerSet and operands.
  explicit IntegerValueSet(const AffineCondition &cond);

  /// Constructs an integer value set from an affine value map.
  // This will lead to a single equality in 'set'.
  explicit IntegerValueSet(const AffineValueMap &avm);

  /// Returns true if this integer set is determined to be empty. Emptiness is
  /// checked by by eliminating identifiers successively (through either
  /// Gaussian or Fourier-Motzkin) while using the GCD test and a trivial
  /// constraint check. Returns 'true' if the constaint system is found to be
  /// empty; false otherwise.
  bool isEmpty() const;

  bool getNumDims() const { return set.getNumDims(); }
  bool getNumSymbols() const { return set.getNumSymbols(); }

private:
  // The set pointed to may itself change unlike in IR structures like
  // 'AffineCondition'.
  MutableIntegerSet set;
  /// The SSA operands binding to the dim's and symbols of 'set'.
  SmallVector<MLValue *, 4> operands;
};

/// A flat list of affine equalities and inequalities in the form.
/// Inequality: c_0*x_0 + c_1*x_1 + .... + c_{n-1}*x_{n-1} == 0
/// Equality: c_0*x_0 + c_1*x_1 + .... + c_{n-1}*x_{n-1} >= 0
///
/// FlatAffineConstraints stores coefficients in a contiguous buffer (one buffer
/// for equalities and one for inequalities). The size of each buffer is
/// numReservedCols * number of inequalities (or equalities). The reserved size
/// is numReservedCols * numReservedInequalities (or numReservedEqualities). A
/// coefficient (r, c) lives at the location numReservedCols * r + c in the
/// buffer. The extra space between getNumCols() and numReservedCols exists to
/// prevent frequent movement of data when adding columns, especially at the
/// end.
///
/// The identifiers x_0, x_1, ... appear in the order: dimensional identifiers,
/// symbolic identifiers, and local identifiers.  The local identifiers
/// correspond to local/internal variables created temporarily when converting
/// from tree AffineExpr's that have mod's and div's and are thus needed
/// to increase representational power.
//
class FlatAffineConstraints {
public:
  enum IdKind { Dimension, Symbol, Local };

  /// Constructs a constraint system reserving memory for the specified number
  /// of constraints and identifiers..
  FlatAffineConstraints(unsigned numReservedInequalities,
                        unsigned numReservedEqualities,
                        unsigned numReservedCols, unsigned numDims = 0,
                        unsigned numSymbols = 0, unsigned numLocals = 0,
                        ArrayRef<Optional<MLValue *>> idArgs = {})
      : numReservedCols(numReservedCols), numDims(numDims),
        numSymbols(numSymbols) {
    assert(numReservedCols >= numDims + numSymbols + 1);
    assert(idArgs.empty() || idArgs.size() == numDims + numSymbols + numLocals);
    equalities.reserve(numReservedCols * numReservedEqualities);
    inequalities.reserve(numReservedCols * numReservedInequalities);
    numIds = numDims + numSymbols + numLocals;
    ids.reserve(numReservedCols);
    if (idArgs.empty())
      ids.resize(numIds, None);
    else
      ids.insert(ids.end(), idArgs.begin(), idArgs.end());
  }

  /// Constructs a constraint system with the specified number of
  /// dimensions and symbols.
  FlatAffineConstraints(unsigned numDims = 0, unsigned numSymbols = 0,
                        unsigned numLocals = 0,
                        ArrayRef<Optional<MLValue *>> idArgs = {})
      : numReservedCols(numDims + numSymbols + numLocals + 1), numDims(numDims),
        numSymbols(numSymbols) {
    assert(numReservedCols >= numDims + numSymbols + 1);
    assert(idArgs.empty() || idArgs.size() == numDims + numSymbols + numLocals);
    numIds = numDims + numSymbols + numLocals;
    ids.reserve(numIds);
    if (idArgs.empty())
      ids.resize(numIds, None);
    else
      ids.insert(ids.end(), idArgs.begin(), idArgs.end());
  }

  explicit FlatAffineConstraints(const HyperRectangularSet &set);

  /// Create a flat affine constraint system from an AffineValueMap or a list of
  /// these. The constructed system will only include equalities.
  // TODO(bondhugula)
  explicit FlatAffineConstraints(const AffineValueMap &avm);
  explicit FlatAffineConstraints(ArrayRef<const AffineValueMap *> avmRef);

  /// Creates an affine constraint system from an IntegerSet.
  explicit FlatAffineConstraints(IntegerSet set);

  /// Create an affine constraint system from an IntegerValueSet.
  // TODO(bondhugula)
  explicit FlatAffineConstraints(const IntegerValueSet &set);

  FlatAffineConstraints(const FlatAffineConstraints &other);

  FlatAffineConstraints(ArrayRef<const AffineValueMap *> avmRef,
                        IntegerSet set);

  FlatAffineConstraints(const MutableAffineMap &map);

  ~FlatAffineConstraints() {}

  // Clears any existing data and reserves memory for the specified constraints.
  void reset(unsigned numReservedInequalities, unsigned numReservedEqualities,
             unsigned numReservedCols, unsigned numDims, unsigned numSymbols,
             unsigned numLocals = 0, ArrayRef<MLValue *> idArgs = {});

  void reset(unsigned numDims = 0, unsigned numSymbols = 0,
             unsigned numLocals = 0, ArrayRef<MLValue *> idArgs = {});

  /// Appends constraints from 'other' into this. This is equivalent to an
  /// intersection with no simplification of any sort attempted.
  void append(const FlatAffineConstraints &other);

  // Checks for emptiness by performing variable elimination on all identifiers,
  // running the GCD test on each equality constraint, and checking for invalid
  // constraints.
  // Returns true if the GCD test fails for any equality, or if any invalid
  // constraints are discovered on any row. Returns false otherwise.
  bool isEmpty() const;

  // Runs the GCD test on all equality constraints. Returns 'true' if this test
  // fails on any equality. Returns 'false' otherwise.
  // This test can be used to disprove the existence of a solution. If it
  // returns true, no integer solution to the equality constraints can exist.
  bool isEmptyByGCDTest() const;

  // Clones this object.
  std::unique_ptr<FlatAffineConstraints> clone() const;

  /// Returns the value at the specified equality row and column.
  inline int64_t atEq(unsigned i, unsigned j) const {
    return equalities[i * numReservedCols + j];
  }
  inline int64_t &atEq(unsigned i, unsigned j) {
    return equalities[i * numReservedCols + j];
  }

  inline int64_t atIneq(unsigned i, unsigned j) const {
    return inequalities[i * numReservedCols + j];
  }

  inline int64_t &atIneq(unsigned i, unsigned j) {
    return inequalities[i * numReservedCols + j];
  }

  /// Returns the number of columns in the constraint system.
  inline unsigned getNumCols() const { return numIds + 1; }

  inline unsigned getNumEqualities() const {
    assert(equalities.size() % numReservedCols == 0 &&
           "inconsistent equality buffer size");
    return equalities.size() / numReservedCols;
  }

  inline unsigned getNumInequalities() const {
    assert(inequalities.size() % numReservedCols == 0 &&
           "inconsistent inequality buffer size");
    return inequalities.size() / numReservedCols;
  }

  inline unsigned getNumReservedEqualities() const {
    return equalities.capacity() / numReservedCols;
  }

  inline unsigned getNumReservedInequalities() const {
    return inequalities.capacity() / numReservedCols;
  }

  inline ArrayRef<int64_t> getEquality(unsigned idx) const {
    return ArrayRef<int64_t>(&equalities[idx * numReservedCols], getNumCols());
  }

  inline ArrayRef<int64_t> getInequality(unsigned idx) const {
    return ArrayRef<int64_t>(&inequalities[idx * numReservedCols],
                             getNumCols());
  }

  AffineExpr toAffineExpr(unsigned idx, MLIRContext *context);

  // Returns an AffineMap that expresses the identifier at pos as a function of
  // other dimensional and symbolic identifiers using the 'idx^th' equality
  // constraint.
  // If 'nonZeroDimIds' and 'nonZeroSymbolIds' are non-null, they are populated
  // with the positions of the non-zero equality constraint coefficients which
  // were used to build the returned AffineMap.
  // Returns AffineMap::Null on error (i.e. if coefficient is zero or does
  // not divide other coefficients in the equality constraint).
  // TODO(andydavis) Remove 'nonZeroDimIds' and 'nonZeroSymbolIds' from this
  // API when we can manage the mapping of MLValues and ids in the constraint
  // system.
  AffineMap toAffineMapFromEq(unsigned idx, unsigned pos, MLIRContext *context,
                              SmallVectorImpl<unsigned> *nonZeroDimIds,
                              SmallVectorImpl<unsigned> *nonZeroSymbolIds);

  // Adds an inequality (>= 0) from the coefficients specified in inEq.
  void addInequality(ArrayRef<int64_t> inEq);
  // Adds an equality from the coefficients specified in eq.
  void addEquality(ArrayRef<int64_t> eq);

  /// Adds a constant lower bound constraint for the specified identifier.
  void addConstantLowerBound(unsigned pos, int64_t lb);
  /// Adds a constant upper bound constraint for the specified identifier.
  void addConstantUpperBound(unsigned pos, int64_t ub);

  /// Adds a lower bound expression for the specified expression.
  void addLowerBound(ArrayRef<int64_t> expr, ArrayRef<int64_t> lb);

  /// Adds constraints (lower and upper bounds) for the specified 'for'
  /// statement's MLValue using IR information stored in its bound maps. The
  /// right identifier is first looked up using forStmt's MLValue. Returns
  /// false for the yet unimplemented/unsupported cases, and true if the
  /// information is succesfully added. Asserts if the MLValue corresponding to
  /// the 'for' statement isn't found in the constraint system. Any new
  /// identifiers that are found in the bound operands of the 'for' statement
  /// are added as trailing identifiers (either dimensional or symbolic
  /// depending on whether the operand is a valid MLFunction symbol).
  //  TODO(bondhugula): add support for non-unit strides.
  bool addForStmtDomain(const ForStmt &forStmt);

  /// Adds an upper bound expression for the specified expression.
  void addUpperBound(ArrayRef<int64_t> expr, ArrayRef<int64_t> ub);

  /// Adds a constant lower bound constraint for the specified expression.
  void addConstantLowerBound(ArrayRef<int64_t> expr, int64_t lb);
  /// Adds a constant upper bound constraint for the specified expression.
  void addConstantUpperBound(ArrayRef<int64_t> expr, int64_t ub);

  /// Sets the identifier at the specified position to a constant.
  void setIdToConstant(unsigned pos, int64_t val);

  /// Sets the identifier corresponding to the specified MLValue id to a
  /// constant. Asserts if the 'id' is not found.
  void setIdToConstant(const MLValue &id, int64_t val);

  /// Looks up the identifier with the specified MLValue. Returns false if not
  /// found, true if found. pos is set to the (column) position of the
  /// identifier.
  bool findId(const MLValue &id, unsigned *pos) const;

  // Add identifiers of the specified kind - specified positions are relative to
  // the kind of identifier. 'id' is the MLValue corresponding to the
  // identifier that can optionally be provided.
  void addDimId(unsigned pos, MLValue *id = nullptr);
  void addSymbolId(unsigned pos, MLValue *id = nullptr);
  void addLocalId(unsigned pos);
  void addId(IdKind kind, unsigned pos, MLValue *id = nullptr);

  /// Composes the affine value map with this FlatAffineConstrains, adding the
  /// results of the map as dimensions at the front [0, vMap->getNumResults())
  /// and with the dimensions set to the equalities specified by the value map.
  /// Returns false if the composition fails (when vMap is a semi-affine map).
  /// The vMap's operand MLValue's are used to look up the right positions in
  /// the FlatAffineConstraints with which to associate. The dimensional and
  /// symbolic operands of vMap should match 1:1 (in the same order) with those
  /// of this constraint system, but the latter could have additional trailing
  /// operands.
  bool composeMap(AffineValueMap *vMap);

  /// Projects out (aka eliminates) 'num' identifiers starting at position
  /// 'pos'. The resulting constraint system is the shadow along the dimensions
  /// that still exist. This method may not always be integer exact.
  // TODO(bondhugula): deal with integer exactness when necessary - can return a
  // value to mark exactness for example.
  void projectOut(unsigned pos, unsigned num);
  inline void projectOut(unsigned pos) { return projectOut(pos, 1); }

  /// Projects out the identifier that is associate with MLValue *.
  void projectOut(MLValue *id);

  void removeId(IdKind idKind, unsigned pos);
  void removeId(unsigned pos);

  void removeDim(unsigned pos);

  void removeEquality(unsigned pos);
  void removeInequality(unsigned pos);

  /// Changes the partition between dimensions and symbols. Depending on the new
  /// symbol count, either a chunk of trailing dimensional identifiers becomes
  /// symbols, or some of the leading symbols become dimensions.
  void setDimSymbolSeparation(unsigned newSymbolCount);

  /// Sets the specified identifier to a constant and removes it.
  void setAndEliminate(unsigned pos, int64_t constVal);

  /// Tries to fold the specified identifer to a constant using a trivial
  /// equality detection; if successful, the constant is substituted for the
  /// identifier everywhere in the constraint system and then removed from the
  /// system. Returns true if the folding happens, false otherwise.
  bool constantFoldId(unsigned pos);

  /// This method calls constantFoldId for the specified range of identifiers,
  /// 'num' identifiers starting at position 'pos'.
  void constantFoldIdRange(unsigned pos, unsigned num);

  unsigned getNumConstraints() const {
    return getNumInequalities() + getNumEqualities();
  }
  inline unsigned getNumIds() const { return numIds; }
  inline unsigned getNumDimIds() const { return numDims; }
  inline unsigned getNumSymbolIds() const { return numSymbols; }
  inline unsigned getNumLocalIds() const {
    return numIds - numDims - numSymbols;
  }

  inline ArrayRef<Optional<MLValue *>> getIds() const {
    return {ids.data(), ids.size()};
  }

  /// Returns the MLValue's associated with the identifiers. Asserts if
  /// no MLValue was associated with an identifier.
  inline void getIdValues(SmallVectorImpl<MLValue *> *values) const {
    values->clear();
    values->reserve(numIds);
    for (unsigned i = 0; i < numIds; i++) {
      assert(ids[i].hasValue() && "identifier's MLValue not set");
      values->push_back(ids[i].getValue());
    }
  }

  /// Returns the MLValue associated with the pos^th identifier. Asserts if
  /// no MLValue identifier was associated.
  inline MLValue *getIdValue(unsigned pos) const {
    assert(ids[pos].hasValue() && "identifier's ML Value not set");
    return ids[pos].getValue();
  }

  /// Clears this list of constraints and copies other into it.
  void clearAndCopyFrom(const FlatAffineConstraints &other);

  /// Returns the smallest known constant bound for the extent of the
  /// specified identifier (pos^th), i.e., the smallest known constant that is
  /// greater than or equal to 'exclusive upper bound' - 'lower bound' of the
  /// identifier; returns None if it's not a constant. This method employs
  /// trivial (low complexity / cost) checks and detection. Symbolic identifiers
  /// are treated specially, i.e., it looks for constant differences between
  /// affine expressions involving only the symbolic identifiers. See comments
  /// at function definition for examples. 'lb', if provided, is set to the
  /// lower bound associated with the constant difference.
  Optional<int64_t>
  getConstantBoundOnDimSize(unsigned pos,
                            SmallVectorImpl<int64_t> *lb = nullptr) const;

  /// Returns true if the set can be trivially detected as being
  /// hyper-rectangular on the specified contiguous set of identifiers.
  bool isHyperRectangular(unsigned pos, unsigned num) const;

  /// Removes duplicates and trivially true constraints: a constraint of the
  /// form <non-negative constant> >= 0 is considered a trivially true
  /// constraint.
  void removeTrivialRedundancy();

  void print(raw_ostream &os) const;
  void dump() const;

private:
  /// Returns false if the fields corresponding to various identifier counts, or
  /// equality/inequality buffer sizes aren't consistent; true otherwise. This
  /// is meant to be used within an assert internally.
  bool hasConsistentState() const;

  /// Checks all rows of equality/inequality constraints for trivial
  /// contradictions (for example: 1 == 0, 0 >= 1), which may have surfaced
  /// after elimination. Returns 'true' if an invalid constraint is found;
  /// 'false'otherwise.
  bool hasInvalidConstraint() const;

  // Eliminates a single identifier at 'position' from equality and inequality
  // constraints. Returns 'true' if the identifier was eliminated, and false
  // otherwise.
  inline bool gaussianEliminateId(unsigned position) {
    return gaussianEliminateIds(position, position + 1) == 1;
  }

  // Eliminates identifiers from equality and inequality constraints
  // in column range [posStart, posLimit).
  // Returns the number of variables eliminated.
  unsigned gaussianEliminateIds(unsigned posStart, unsigned posLimit);

  /// Eliminates identifier at the specified position using Fourier-Motzkin
  /// variable elimination, but uses Gaussian elimination if there is an
  /// equality involving that identifier. If the result of the elimination is
  /// integer exact, *isResultIntegerExact is set to true. If 'darkShadow' is
  /// set to true, a potential under approximation (subset) of the rational
  /// shadow / exact integer shadow is computed.
  // See implementation comments for more details.
  void FourierMotzkinEliminate(unsigned pos, bool darkShadow = false,
                               bool *isResultIntegerExact = nullptr);

  /// Tightens inequalities given that we are dealing with integer spaces. This
  /// is similar to the GCD test but applied to inequalities. The constant term
  /// can be reduced to the preceding multiple of the GCD of the coefficients,
  /// i.e.,
  ///  64*i - 100 >= 0  =>  64*i - 128 >= 0 (since 'i' is an integer). This is a
  /// fast method (linear in the number of coefficients).
  void GCDTightenInequalities();

  /// Normalized each constraints by the GCD of its coefficients.
  void normalizeConstraintsByGCD();

  /// Removes identifiers in column range [idStart, idLimit), and copies any
  /// remaining valid data into place, updates member variables, and resizes
  /// arrays as needed.
  void removeIdRange(unsigned idStart, unsigned idLimit);

  /// Coefficients of affine equalities (in == 0 form).
  SmallVector<int64_t, 64> equalities;

  /// Coefficients of affine inequalities (in >= 0 form).
  SmallVector<int64_t, 64> inequalities;

  /// Number of columns reserved. Actual ones in used are returned by
  /// getNumCols().
  unsigned numReservedCols;

  /// Total number of identifiers.
  unsigned numIds;

  /// Number of identifiers corresponding to real dimensions.
  unsigned numDims;

  /// Number of identifiers corresponding to symbols (unknown but constant for
  /// analysis).
  unsigned numSymbols;

  /// MLValues corresponding to the (column) identifiers of this constraint
  /// system appearing in the order the identifiers correspond to columns.
  /// Temporary ones or those that aren't associated to any MLValue are to be
  /// set to None.
  SmallVector<Optional<MLValue *>, 8> ids;
};

} // end namespace mlir.

#endif // MLIR_ANALYSIS_AFFINE_STRUCTURES_H
