"""The shared finite LP rows used by Python and native solver comparisons."""

import numpy as np
from outshine_ref.transport.heights import _gradient_weights
from scipy import sparse


def linear_system(mesh, flow, rows, down_grade, cross_grade):
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    weights = _gradient_weights(mesh.vertices[:, :2], mesh.triangles)
    cross = np.c_[-flow[:, 1], flow[:, 0]]
    row_faces = rows[:, 0].astype(int)
    direction = rows[:, 1:3] / np.linalg.norm(rows[:, 1:3], axis=1)[:, None]

    def matrix(coefficients, faces=mesh.triangles):
        return sparse.coo_matrix(
            (
                coefficients.ravel(),
                (np.repeat(np.arange(len(faces)), 3), faces.ravel()),
            ),
            shape=(len(faces), nv + 1),
        ).tocsr()

    along = matrix(np.einsum("fi,fij->fj", flow, weights))
    across = matrix(np.einsum("fi,fij->fj", cross, weights))
    directed = matrix(
        np.einsum("fi,fij->fj", direction, weights[row_faces]),
        mesh.triangles[row_faces],
    )
    eye = sparse.hstack([sparse.eye(nv), sparse.csr_matrix((nv, 1))])
    error = sparse.coo_matrix(
        (-np.ones(nv), (np.arange(nv), np.full(nv, nv))), shape=(nv, nv + 1)
    ).tocsr()
    datum = float(np.median(mesh.vertices[:, 2]))
    target = mesh.vertices[:, 2] - datum
    constraints = sparse.vstack(
        [
            eye + error,
            -eye + error,
            along,
            -along,
            across,
            -across,
            directed,
            -directed,
        ],
        format="csr",
    )
    constraints.eliminate_zeros()
    bounds = np.r_[
        target,
        -target,
        np.zeros(nf),
        np.full(nf, down_grade),
        np.full(nf, cross_grade),
        np.full(nf, cross_grade),
        rows[:, 4],
        -rows[:, 3],
    ]
    return constraints, bounds, datum, target
