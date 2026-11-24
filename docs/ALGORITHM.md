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

## 5. What this is / is not

- **Is:** Open, documented ISO-concept second moments + hyperbolic \(M^2\) + configurable coarse/fine scan hooks suitable for engineering use and unit testing.
- **Is not:** A claim of accredited ISO 11146 certification, nor a dump of any commercial beam-profiler source code. Normative purchases of ISO standards remain the user’s responsibility for compliance work.

## 6. Source map

| Component | Headers / sources |
|-----------|-------------------|
| Moments + noise | `Iso11146.hpp`, `MomentOptions.hpp`, `Roi.hpp` |
| \(M^2\) fit | `M2Fit.hpp` / `M2Fit.cpp` |
| Multi-axis scan result | `M2Scan.hpp` / `M2Scan.cpp` |
| Motor plan + session | `MotorScan.hpp` / `MotorScan.cpp` |
| CLI | `apps/beam_m2_scan` |
