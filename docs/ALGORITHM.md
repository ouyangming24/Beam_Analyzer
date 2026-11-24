# Beam Analyzer — Algorithms & Measurement Practice

This document summarizes the **second-moment**, **M² hyperbolic fit**, and **motor z-scan** methods implemented in `libs/core`. It is intentionally non-proprietary: concepts follow open standards and common laboratory practice for laser beam characterization. No Cinogy (or other vendor) internal algorithms are reverse-engineered or required.

## 1. Second-moment beam width (ISO 11146 family)

### 1.1 Definition

For a planar intensity distribution \(I(x,y)\), the first-moment (centroid) and second-moment variances are:

\[
\langle x\rangle = \frac{\iint x\,I\,\mathrm{d}x\,\mathrm{d}y}{\iint I\,\mathrm{d}x\,\mathrm{d}y},\quad
\sigma_x^2 = \frac{\iint (x-\langle x\rangle)^2 I\,\mathrm{d}x\,\mathrm{d}y}{\iint I\,\mathrm{d}x\,\mathrm{d}y}
\]

(and likewise for \(y\), plus cross-moment \(\sigma_{xy}\)). The **ISO second-moment diameter** is:

\[
d_\sigma = 4\sigma
\]

Principal-axis diameters follow from diagonalizing the \(2\times 2\) covariance matrix; ellipticity is \(d_\mathrm{minor}/d_\mathrm{major}\).

**References (concepts):**

- ISO 11146-1, *Lasers and laser-related equipment — Test methods for laser beam widths, divergence angles and beam propagation ratios — Part 1: Stigmatic and simple astigmatic beams*.
- ISO 11146-2, *… Part 2: General astigmatic beams* (twist / general second-moment tensor concepts).
- Siegman, A. E., “How to (Maybe) Measure Laser Beam Quality,” OSA TOPS Vol. 17 (tutorial discussion of \(M^2\) and second moments).

### 1.2 Background & noise handling (implemented options)

Commercial / lab rigor usually separates **background estimation** from the moment definition itself:

| Option | API | Typical use |
|--------|-----|-------------|
| No background | `BackgroundMode::None` | Clean synthetic / already corrected frames |
| Constant offset | `BackgroundMode::Constant` | Dark-level from separate measurement |
| Border mean | `BackgroundMode::BorderMean` | Frame-edge strip as ambient estimate |
| Background ROI | `BackgroundMode::RoiMean` | Dedicated dark region |
| Clip negatives | `clipNegative` | After subtraction, enforce \(I\ge 0\) |
| Absolute noise floor | `NoiseFloorMode::Absolute` | Zero pixels below fixed DN |
| Fraction of peak | `NoiseFloorMode::FractionOfPeak` | e.g. suppress &lt;1% of peak |
| \(n\sigma\) above noise | `NoiseFloorMode::SigmaAboveBg` | Gate using estimated border \(\sigma\) |

Entry point: `computeIso11146(frame, MomentOptions)` after `prepareAnalysisFrame` / `applyNoiseFloor`.

**Practice notes:** Over-aggressive thresholding can bias \(d_\sigma\) low for low-SNR beams; under-subtraction biases widths high. Prefer measured dark frames when available. ISO procedures emphasize background correction before moments; pixel gates are a practical lab control, not a replacement for the \(4\sigma\) definition.

## 2. Hyperbolic \(M^2\) fit

### 2.1 Propagation model

For a stigmatic (or separately treated) axis, the second-moment diameter along the propagation axis obeys a **hyperbola**:

\[
d^2(z) = d_0^2\left[1+\left(\frac{z-z_0}{z_R}\right)^2\right],\qquad
z_R = \frac{\pi w_0^2}{M^2\lambda},\qquad w_0=\frac{d_0}{2}.
\]

Equivalently, a least-squares fit of \(d^2(z)=d_0^2 + a(z-z_0)^2\) recovers \(M^2\) from \(a\), \(d_0\), and \(\lambda\).

**References (concepts):**

- ISO 11146-1 § beam propagation ratio / \(M^2\) evaluation from multi-plane diameters.
- ISO 11146-3 (informative / guidance parts in the series) on measurement arrangements — consult the published standard for normative clauses.
- Classic textbook treatments of Gaussian / embedded-Gaussian propagation (e.g. Siegman, *Lasers*).

### 2.2 Implementation (`fitM2`)

1. Validate ≥3 samples; optional weights.
2. Coarse grid search over \((z_0,d_0)\); solve \(a\) analytically (weighted LS).
3. Local refine around the coarse minimum.
4. Convert \(a\to M^2\); compute Rayleigh range \(z_R\).
5. Emit **per-sample residuals** (diameter and \(d^2\)), RMS / relative RMS.
6. **Uncertainty heuristic** from residual variance and finite-difference curvature of the residual sum of squares (order-of-magnitude \(1\sigma\), not a full covariance MCMC).
7. **Quality flags** (bitmask): too few points, narrow \(z\) span vs \(z_R\), missing near-waist sample, high residual, non-physical \(M^2\), one-sided \(z\) coverage.

Multi-axis: `analyzeM2Scan` fits **X**, **Y**, and a combined \(d=\sqrt{(d_x^2+d_y^2)/2}\) curve; `analyzeM2ScanSingleAxis` fits one axis only.

## 3. Motor scan workflow (coarse → fine)

### 3.1 Why two stages?

ISO-style \(M^2\) needs enough planes spanning on the order of the Rayleigh range, with **coverage near the waist**. Lab practice often:

1. **Coarse scan** over a wide \(z\) window (large step) to locate the approximate waist.
2. **Fine scan** ± a few \(z_R\) (or a user mm window) with a smaller step for fit quality.

### 3.2 API

- `planCoarsePositions` / `planMotorScan` — user-configurable ranges & steps; position list generation.
- `IStage` / `SimulatedStage` — move absolute \(z\); simulated stage is sufficient without proprietary motion controllers.
- `AcquireAtZFn` — acquisition hook (camera, CSV playback, or synthetic).
- `runScanSession` — coarse → estimate waist → fine → full multi-axis fit.
- CLI: `beam_m2_scan` exports Markdown/JSON report + `D(z)` CSV and prints an ASCII plot.
- GUI: M² dialog can generate a plan and run a simulated session, then plot / export.

## 4. Units

| Quantity | Unit in API |
|----------|-------------|
| Lateral size / diameter | µm |
| Pixel pitch | µm |
| Propagation \(z\) | mm |
| Wavelength | nm |

Conversions inside the fit keep \(\lambda\), \(w_0\), and \(z\) consistent (µm internally for the Rayleigh formula).

## 5. Area of Interest (AOI)

An AOI restricts which pixels contribute to analysis. Shapes:

| Shape | Containment |
|-------|-------------|
| Rect | Inclusive axis-aligned box |
| Circle | \((x-c_x)^2+(y-c_y)^2 \le r^2\) with \(r=\min(r_x,r_y)\) from the bounding box |
| Ellipse | \(\bigl((x-c_x)/r_x\bigr)^2+\bigl((y-c_y)/r_y\bigr)^2 \le 1\) |

`applyAoiMask` zeros intensity outside the AOI **without cropping**, so centroids remain in full-frame coordinates (µm from the detector origin). Auto-AOI places a region of half-width \(\mathrm{scale}\cdot d_\sigma/2\) about the second-moment centroid (`autoAoiFromCentroid`), i.e. AOI axis length \(\approx \mathrm{scale}\cdot d_\sigma\) (default \(\mathrm{scale}=1.5\) for a tight enclosure around the ISO 4σ beam ellipse; increase toward 2–3 when more background margin is needed).

Legacy `useRoi` crop path remains for backward-compatible rectangular ROI workflows.

## 6. Uniformity & flatness (ISO 13694 concepts)

On a prepared intensity map \(I(x,y)\), let \(I_\mathrm{peak}=\max I\) and choose a fraction \(\eta\in(0,1]\) (default \(0.5\)). The **effective irradiation area** \(A_\eta\) is the set of pixels with \(I\ge\eta\,I_\mathrm{peak}\).

\[
\bar I_\eta=\langle I\rangle_{A_\eta},\qquad
F=\frac{I_\mathrm{peak}}{\bar I_\eta}\quad(\text{flatness factor}\ge 1),\qquad
F_\mathrm{norm}=\frac{\bar I_\eta}{I_\mathrm{peak}}
\]

\[
U=1-\frac{\sigma_\eta}{\bar I_\eta},\qquad
U_{\mathrm{minmax}}=1-\frac{I_\mathrm{peak}-I_{\min,\eta}}{I_\mathrm{peak}+I_{\min,\eta}}
\]

where \(\sigma_\eta\) is the RMS deviation of \(I\) on \(A_\eta\). Entry: `computeIso13694(frame, eta)`.

**References (concepts):** ISO 13694, *Optics and optical instruments — Lasers and laser-related equipment — Test methods for laser beam power (energy) density distribution*.

## 7. Divergence & Beam Parameter Product (BPP)

From a hyperbolic \(M^2\) fit with waist radius \(w_0=d_0/2\) and Rayleigh range \(z_R\):

\[
\theta=\frac{w_0}{z_R}\quad(\text{half-angle, rad}),\qquad
\mathrm{BPP}=w_0\,\theta
\]

Reported as mrad and mm·mrad respectively (`divergenceFromM2Fit`). Full angle ≈ \(2\theta\). These are engineering diagnostics derived from the same embedded-Gaussian model as the \(M^2\) fit — **not** a standalone ISO divergence certificate.

## 8. Position & pointing stability (ISO 11670 concepts)

Given a time series of centroids \((x_i,y_i)\) (µm) and optional power:

\[
\sigma_x=\mathrm{std}(x),\quad \sigma_y=\mathrm{std}(y),\quad
\Delta r=2\sqrt{\sigma_x^2+\sigma_y^2}
\]

With an effective focal length \(f\) (mm), angular pointing spreads are

\[
\sigma_{\theta x}\approx\frac{\sigma_x}{f}\ \mathrm{mrad},\quad
\sigma_{\theta y}\approx\frac{\sigma_y}{f}\ \mathrm{mrad}
\]

(using \(\sigma\) in µm and \(f\) in mm so that \(\sigma/f\) is already in mrad). Entry: `computeStability(samples, focalLength_mm)`.

**References (concepts):** ISO 11670, *… Test methods for laser beam parameters — Beam positional stability*.

## 9. What this is / is not

- **Is:** Open, documented ISO-concept second moments + hyperbolic \(M^2\) + AOI masking + ISO 13694/11670–style engineering metrics + configurable coarse/fine scan hooks suitable for engineering use and unit testing.
- **Is not:** A claim of accredited ISO 11146 / 13694 / 11670 certification, nor a dump of any commercial beam-profiler source code or brand assets. Normative purchases of ISO standards remain the user’s responsibility for compliance work.

## 10. Source map

| Component | Headers / sources |
|-----------|-------------------|
| Moments + noise | `Iso11146.hpp`, `MomentOptions.hpp`, `Roi.hpp` |
| AOI shapes / mask | `Aoi.hpp` / `Aoi.cpp` |
| Uniformity / flatness | `Iso13694.hpp` / `Iso13694.cpp` |
| Stability | `Stability.hpp` / `Stability.cpp` |
| Divergence / BPP | `Divergence.hpp` / `Divergence.cpp` |
| \(M^2\) fit | `M2Fit.hpp` / `M2Fit.cpp` |
| Multi-axis scan result | `M2Scan.hpp` / `M2Scan.cpp` |
| Motor plan + session | `MotorScan.hpp` / `MotorScan.cpp` |
| CLI | `apps/beam_m2_scan` |
