#pragma once

#include "beam/core/BeamFrame.hpp"
#include "beam/core/Iso11146.hpp"
#include "beam/core/Iso13694.hpp"
#include "beam/core/PowerMetrics.hpp"
#include "beam/core/Stability.hpp"
#include "beam/core/Aoi.hpp"

#include <QDialog>

#include <optional>
#include <vector>

class QTreeWidget;
class QLabel;
class QTabWidget;
class QDoubleSpinBox;

namespace beam::ui {

class ImageView;
class IntensitySurface3D;
class ProfilePlot;

/// Cinogy-style complete analysis workstation (layout/workflow inspired only).
/// Engineering preview — not a metrology / ISO certification claim.
class AnalysisDialog final : public QDialog {
  Q_OBJECT
 public:
  explicit AnalysisDialog(QWidget* parent = nullptr);

  void setFrame(const beam::core::BeamFrame& frame,
                const beam::core::Iso11146Metrics& metrics,
                const std::optional<beam::core::PowerMetrics>& power,
                const std::optional<beam::core::AoiRegion>& aoi,
                double wavelength_nm);

  void pushStabilitySample(double t_s,
                           double cx_um,
                           double cy_um,
                           double power);

 private:
  void rebuildParamTree(const beam::core::Iso11146Metrics& metrics,
                        const std::optional<beam::core::PowerMetrics>& power,
                        const beam::core::Iso13694Metrics& iso13694);
  void refreshEnergyPage(const beam::core::Iso13694Metrics& iso13694);
  void refreshStabilityPage();
  void refreshPointingPage();
  void refreshM2BppPage();

  QTabWidget* tabs_ = nullptr;
  ImageView* spotView_ = nullptr;
  QTreeWidget* paramTree_ = nullptr;
  IntensitySurface3D* surface3d_ = nullptr;
  ProfilePlot* energyProfile_ = nullptr;
  QLabel* uniformityBig_ = nullptr;
  QLabel* flatnessBig_ = nullptr;
  ProfilePlot* stabilityXPlot_ = nullptr;
  ProfilePlot* stabilityYPlot_ = nullptr;
  ProfilePlot* stabilityPPlot_ = nullptr;
  ProfilePlot* pointingScatter_ = nullptr;
  QTreeWidget* stabilityTree_ = nullptr;
  QTreeWidget* pointingTree_ = nullptr;
  QTreeWidget* m2Tree_ = nullptr;
  QDoubleSpinBox* etaSpin_ = nullptr;
  QDoubleSpinBox* focalSpin_ = nullptr;
  QLabel* disclaimer_ = nullptr;

  std::optional<beam::core::BeamFrame> frame_;
  std::optional<beam::core::Iso11146Metrics> metrics_;
  std::optional<beam::core::PowerMetrics> power_;
  std::optional<beam::core::AoiRegion> aoi_;
  double wavelength_nm_ = 1064.0;
  std::vector<beam::core::StabilitySample> stabilitySamples_;
};

}  // namespace beam::ui
