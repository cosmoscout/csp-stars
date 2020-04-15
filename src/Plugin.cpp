////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Plugin.hpp"

#include "../../../src/cs-core/GraphicsEngine.hpp"
#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-utils/logger.hpp"

#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::stars::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::stars {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, Plugin::Settings& o) {
  cs::core::Settings::deserialize(j, "backgroundTexture1", o.mBackgroundTexture1);
  cs::core::Settings::deserialize(j, "backgroundTexture2", o.mBackgroundTexture2);
  cs::core::Settings::deserialize(j, "backgroundColor1", o.mBackgroundColor1);
  cs::core::Settings::deserialize(j, "backgroundColor2", o.mBackgroundColor2);
  cs::core::Settings::deserialize(j, "starTexture", o.mStarTexture);
  cs::core::Settings::deserialize(j, "cacheFile", o.mCacheFile);
  cs::core::Settings::deserialize(j, "hipparcosCatalog", o.mHipparcosCatalog);
  cs::core::Settings::deserialize(j, "tychoCatalog", o.mTychoCatalog);
  cs::core::Settings::deserialize(j, "tycho2Catalog", o.mTycho2Catalog);
}

void to_json(nlohmann::json& j, Plugin::Settings const& o) {
  cs::core::Settings::serialize(j, "backgroundTexture1", o.mBackgroundTexture1);
  cs::core::Settings::serialize(j, "backgroundTexture2", o.mBackgroundTexture2);
  cs::core::Settings::serialize(j, "backgroundColor1", o.mBackgroundColor1);
  cs::core::Settings::serialize(j, "backgroundColor2", o.mBackgroundColor2);
  cs::core::Settings::serialize(j, "starTexture", o.mStarTexture);
  cs::core::Settings::serialize(j, "cacheFile", o.mCacheFile);
  cs::core::Settings::serialize(j, "hipparcosCatalog", o.mHipparcosCatalog);
  cs::core::Settings::serialize(j, "tychoCatalog", o.mTychoCatalog);
  cs::core::Settings::serialize(j, "tycho2Catalog", o.mTycho2Catalog);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Plugin::Plugin()
    : mProperties(std::make_shared<Properties>()) {

  // Create default logger for this plugin.
  spdlog::set_default_logger(cs::utils::logger::createLogger("csp-stars"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {

  spdlog::info("Loading plugin...");

  // Read star settings.
  mPluginSettings = mAllSettings->mPlugins.at("csp-stars");

  std::map<Stars::CatalogType, std::string> catalogs;

  if (mPluginSettings.mHipparcosCatalog) {
    catalogs[Stars::CatalogType::eHipparcos] = *mPluginSettings.mHipparcosCatalog;
  }

  if (mPluginSettings.mTychoCatalog) {
    catalogs[Stars::CatalogType::eTycho] = *mPluginSettings.mTychoCatalog;
  }

  if (mPluginSettings.mTycho2Catalog) {
    catalogs[Stars::CatalogType::eTycho2] = *mPluginSettings.mTycho2Catalog;
  }

  std::string cacheFile = "star_cache.dat";
  if (mPluginSettings.mCacheFile) {
    cacheFile = *mPluginSettings.mCacheFile;
  }

  // Create the Stars object based on the settings.
  mStars = std::make_unique<Stars>(catalogs, mPluginSettings.mStarTexture, cacheFile);

  // Configure the stars based on some additional settings.
  auto& bg1 = mPluginSettings.mBackgroundColor1;
  auto& bg2 = mPluginSettings.mBackgroundColor2;

  mStars->setBackgroundTexture1(mPluginSettings.mBackgroundTexture1);
  mStars->setBackgroundTexture2(mPluginSettings.mBackgroundTexture2);

  mStars->setBackgroundColor1(VistaColor(bg1.r, bg1.g, bg1.b, bg1.a));
  mStars->setBackgroundColor2(VistaColor(bg2.r, bg2.g, bg2.b, bg2.a));

  // Add the stars to the scenegraph.
  mStarsTransform = std::make_shared<cs::scene::CelestialAnchorNode>(
      mSceneGraph->GetRoot(), mSceneGraph->GetNodeBridge(), "", "Solar System Barycenter", "J2000");
  mSolarSystem->registerAnchor(mStarsTransform);

  mSceneGraph->GetRoot()->AddChild(mStarsTransform.get());

  mStarsNode.reset(mSceneGraph->NewOpenGLNode(mStarsTransform.get(), mStars.get()));

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mStarsTransform.get(), static_cast<int>(cs::utils::DrawOrder::eStars));

  // Toggle the stars node when the public property is changed.
  mProperties->mEnabled.connect([this](bool val) { this->mStarsNode->SetIsEnabled(val); });

  // Add the stars user interface components to the CosmoScout user interface.
  mGuiManager->addSettingsSectionToSideBarFromHTML(
      "Stars", "star", "../share/resources/gui/stars_settings.html");

  mGuiManager->addScriptToGuiFromJS("../share/resources/gui/js/csp-stars.js");

  // Register JavaScript callbacks.
  mGuiManager->getGui()->registerCallback("stars.setEnabled",
      "Enables or disables the rendering of stars.",
      std::function([this](bool value) { mProperties->mEnabled = value; }));

  mGuiManager->getGui()->registerCallback("stars.setEnableGrid",
      "If stars are enabled, this enables the rendering of a background grid in celestial "
      "coordinates.",
      std::function([this](bool value) { mProperties->mEnableCelestialGrid = value; }));

  mGuiManager->getGui()->registerCallback("stars.setEnableFigures",
      "If stars are enabled, this enables the rendering of star figures.",
      std::function([this](bool value) { mProperties->mEnableStarFigures = value; }));

  mGuiManager->getGui()->registerCallback("stars.setLuminanceBoost",
      "Adds an artificial brightness boost to the stars.", std::function([this](double value) {
        mProperties->mLuminanceMultiplicator = std::exp(value);
      }));

  mGuiManager->getGui()->registerCallback("stars.setSize",
      "Sets the apparent size of stars on screen.", std::function([this](double value) {
        mStars->setSolidAngle(static_cast<float>(value * 0.0001));
      }));

  mGuiManager->getGui()->registerCallback("stars.setMagnitude",
      "Sets the maximum or minimum magnitude for stars. The first value is the magnitude, the "
      "second determines wich end to set: Zero for the minimum magnitude; one for the maximum "
      "magnitude.",
      std::function([this](double val, double handle) {
        if (handle == 0.0)
          mStars->setMinMagnitude(static_cast<float>(val));
        else
          mStars->setMaxMagnitude(static_cast<float>(val));
      }));

  mGuiManager->getGui()->registerCallback("stars.setDrawMode0",
      "Enables point draw mode for the stars.",
      std::function([this]() { mStars->setDrawMode(Stars::ePoint); }));

  mGuiManager->getGui()->registerCallback("stars.setDrawMode1",
      "Enables smooth point draw mode for the stars.",
      std::function([this]() { mStars->setDrawMode(Stars::eSmoothPoint); }));

  mGuiManager->getGui()->registerCallback("stars.setDrawMode2",
      "Enables disc draw mode for the stars.",
      std::function([this]() { mStars->setDrawMode(Stars::eDisc); }));

  mGuiManager->getGui()->registerCallback("stars.setDrawMode3",
      "Enables smooth disc draw mode for the stars.",
      std::function([this]() { mStars->setDrawMode(Stars::eSmoothDisc); }));

  mGuiManager->getGui()->registerCallback("stars.setDrawMode4",
      "Enables sprite draw mode for the stars.",
      std::function([this]() { mStars->setDrawMode(Stars::eSprite); }));

  mEnableHDRConnection = mAllSettings->mGraphics.pEnableHDR.connectAndTouch(
      [this](bool value) { mStars->setEnableHDR(value); });

  spdlog::info("Loading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  spdlog::info("Unloading plugin...");

  mSolarSystem->unregisterAnchor(mStarsTransform);
  mSceneGraph->GetRoot()->DisconnectChild(mStarsTransform.get());

  mAllSettings->mGraphics.pEnableHDR.disconnect(mEnableHDRConnection);

  mGuiManager->removeSettingsSection("Stars");

  mGuiManager->getGui()->callJavascript("CosmoScout.removeApi", "stars");

  mGuiManager->getGui()->unregisterCallback("stars.setLuminanceBoost");
  mGuiManager->getGui()->unregisterCallback("stars.setSize");
  mGuiManager->getGui()->unregisterCallback("stars.setMagnitude");
  mGuiManager->getGui()->unregisterCallback("stars.setDrawMode0");
  mGuiManager->getGui()->unregisterCallback("stars.setDrawMode1");
  mGuiManager->getGui()->unregisterCallback("stars.setDrawMode2");
  mGuiManager->getGui()->unregisterCallback("stars.setDrawMode3");
  mGuiManager->getGui()->unregisterCallback("stars.setDrawMode4");
  mGuiManager->getGui()->unregisterCallback("stars.setEnabled");
  mGuiManager->getGui()->unregisterCallback("stars.setEnableGrid");
  mGuiManager->getGui()->unregisterCallback("stars.setEnableFigures");

  spdlog::info("Unloading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {

  // Update the stars brightness based on the scene's pApproximateSceneBrightness. This is to fade
  // out the stars when we are close to a Planet. If HDR rendering is enabled, we will not change
  // the star's brightness.
  float fIntensity = mGraphicsEngine->pApproximateSceneBrightness.get();

  if (mAllSettings->mGraphics.pEnableHDR.get()) {
    fIntensity = 1.f;
  }

  mStars->setLuminanceMultiplicator(
      static_cast<float>(fIntensity * (mProperties->mLuminanceMultiplicator.get())));
  mStars->setBackgroundColor1(VistaColor(
      0.5f, 0.8f, 1.f, 0.3f * fIntensity * (mProperties->mEnableCelestialGrid.get() ? 1.f : 0.f)));
  mStars->setBackgroundColor2(VistaColor(
      0.5f, 1.f, 0.8f, 0.3f * fIntensity * (mProperties->mEnableStarFigures.get() ? 1.f : 0.f)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::stars
