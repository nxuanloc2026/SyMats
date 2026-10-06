# Giac adapter

Enable `SYMATS_USE_GIAC` using the dependency setup in `third_party/README.md`.
Link `symats::giac_backend`, then register `std::make_shared<symats::GiacBackend>()`
with `Context::backends()`. The engine and default build remain independent of Giac.

The adapter converts exact integers, rationals, constants, and expression trees
directly to Giac objects. It never parses user text as Giac code. Unknown heads
remain named applications, and `giac_roundtrip()` preserves their structure.
Encoded user names avoid collisions with Giac built-ins. Generated ODE constants
use the first unused names `C1`, `C2`, and so on in the current request.

Supported operations: `Integrate` (including nested bounds), `Limit`, `Series`,
`Solve`, `Factor`, `Simplify`, `Together`, `Apart`, `DSolve`, `Det`, `Inverse`,
`Rank`, `Trace`, `RowReduce`, `Eigenvalues`, `Eigenvectors`, `CharPoly`,
`Transpose`, `LinearSolve`, `MatrixExp`, and vector/matrix `Dot`.

`Solve` and `DSolve` return lists of rule lists. Scalar ODEs use Giac's solver;
first-order linear systems with constant coefficients use its Laplace, linear,
and inverse Laplace operations. System initial conditions currently use zero as
the initial time. Nonlinear systems, variable-coefficient systems, and PDEs
decline so a registered numeric backend can handle them. No numeric solution is
silently substituted for an exact one.

`Series` returns `SeriesData[terms, {x, a, n}]` to preserve the truncation order.
Eigenvector scale and ordering follow Giac; the vectors correspond to the
returned eigenvalue order. Unsupported output types and unevaluated solver
results decline. Backend results carry `unverified` until checked by T-013.

Calls are serialized because Giac uses shared caches. Its diagnostic output is
captured locally rather than printed into a user's notebook.
