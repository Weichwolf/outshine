"""Measured sparse frontend work and a bounded incremental HiGHS model (#217)."""

from dataclasses import dataclass

import numpy as np
from scipy import sparse


class CutLimit(ValueError):
    pass


@dataclass
class Work:
    materializations: int = 0
    materialized_bytes: int = 0
    explicit_value_copy_bytes: int = 0
    submitted_api_bytes: int = 0
    gradient_nonzeros_visited: int = 0
    full_cones_checked: int = 0
    height_nonzeros_visited: int = 0
    equality_nonzeros_visited: int = 0
    support_key_calls: int = 0
    support_key_bytes: int = 0
    maximum_known_sparse_nonzeros: int = 0

    def array(self, value, *, copied=False):
        self.materializations += 1
        self.materialized_bytes += value.nbytes
        if copied:
            self.explicit_value_copy_bytes += value.nbytes
        return value

    def matrix(self, value, *, copied_values=False):
        for array in (value.data, value.indices, value.indptr):
            self.array(array)
        if copied_values:
            self.explicit_value_copy_bytes += value.data.nbytes
        return value

    def check(self, known, maximum):
        if known > maximum:
            raise CutLimit("delayed_lp_sparse_work_budget")
        self.maximum_known_sparse_nonzeros = max(
            self.maximum_known_sparse_nonzeros, known
        )


def check_status(status):
    if str(status).endswith("kError"):
        raise CutLimit("highs_api_error")


class SparseLP:
    def __init__(self, height, equality, shifted, solver, work, maximum, retained):
        import highspy

        self.highs, self.work = highspy.Highs(), work
        self.maximum, self.retained, self.nonzeros = maximum, retained, 0
        settings = {
            "output_flag": False,
            "threads": 1,
            "solver": solver,
            "run_crossover": "off",
            "primal_feasibility_tolerance": 1e-9,
            "dual_feasibility_tolerance": 1e-9,
            "ipm_optimality_tolerance": 1e-9,
            "small_matrix_value": 1e-12,
        }
        for name, value in settings.items():
            check_status(self.highs.setOptionValue(name, value))
        count = height.shape[1] + 1
        cost = work.array(np.zeros(count))
        cost[-1] = 1.0
        lower = work.array(np.full(count, -np.inf))
        lower[-1] = 0.0
        upper = work.array(np.full(count, np.inf))
        starts = work.array(np.zeros(count + 1, np.int32))
        check_status(
            self.highs.addCols(
                count, cost, lower, upper, 0, starts, np.empty(0, np.int32), np.empty(0)
            )
        )
        work.submitted_api_bytes += (
            cost.nbytes + lower.nbytes + upper.nbytes + starts.nbytes
        )
        anticipated = 2 * height.nnz + 2 * height.shape[0] + equality.nnz
        work.check(retained + 2 * anticipated, maximum)
        rho = work.matrix(sparse.csr_matrix(-np.ones((len(shifted), 1))))
        error = work.matrix(
            sparse.vstack(
                [
                    sparse.hstack([height, rho]),
                    sparse.hstack([-height, rho]),
                ]
            ).tocsr()
        )
        self.add(
            error,
            work.array(np.full(error.shape[0], -np.inf)),
            work.array(np.r_[shifted, -shifted]),
        )
        if equality.shape[0]:
            fixed = work.matrix(
                sparse.hstack(
                    [
                        equality,
                        sparse.csr_matrix((equality.shape[0], 1)),
                    ]
                ).tocsr()
            )
            zeros = work.array(np.zeros(fixed.shape[0]))
            self.add(fixed, zeros, zeros)

    def add(self, matrix, lower, upper):
        self.work.check(self.retained + self.nonzeros + 2 * matrix.nnz, self.maximum)
        check_status(
            self.highs.addRows(
                matrix.shape[0],
                lower,
                upper,
                matrix.nnz,
                matrix.indptr,
                matrix.indices,
                matrix.data,
            )
        )
        self.nonzeros = self.highs.getNumNz()
        self.work.submitted_api_bytes += sum(
            a.nbytes
            for a in (
                matrix.data,
                matrix.indices,
                matrix.indptr,
                lower,
                upper,
            )
        )

    def seed_box(self, gradient, margin):
        for begin in range(0, gradient.shape[0], 4096):
            block = self.work.matrix(gradient[begin : begin + 4096], copied_values=True)
            lower = self.work.array(np.full(block.shape[0], -margin))
            upper = self.work.array(np.full(block.shape[0], margin))
            self.add(block, lower, upper)

    def solve(self, remaining, iterations):
        check_status(
            self.highs.setOptionValue("time_limit", self.highs.getRunTime() + remaining)
        )
        check_status(self.highs.setOptionValue("ipm_iteration_limit", iterations))
        check_status(self.highs.setOptionValue("simplex_iteration_limit", iterations))
        check_status(self.highs.run())
        info, solution = self.highs.getInfo(), self.highs.getSolution()
        count = (
            info.ipm_iteration_count
            + info.simplex_iteration_count
            + info.crossover_iteration_count
        )
        values = self.work.array(
            np.array(solution.col_value, float, copy=True), copied=True
        )
        return (
            values if solution.value_valid else None,
            count,
            str(self.highs.getModelStatus()),
        )
