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
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

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

void from_json(const nlohmann::json& j, Plugin::Settings& o) {
  cs::core::parseSection("csp-stars", [&] {
    o.mMinMagnitude       = cs::core::parseProperty<double>("minMagnitude", j);
    o.mMaxMagnitude       = cs::core::parseProperty<double>("maxMagnitude", j);
    o.mMinSize            = cs::core::parseProperty<double>("minSize", j);
    o.mMaxSize            = cs::core::parseProperty<double>("maxSize", j);
    o.mMinOpacity         = cs::core::parseProperty<double>("minOpacity", j);
    o.mMaxOpacity         = cs::core::parseProperty<double>("maxOpacity", j);
    o.mScalingExponent    = cs::core::parseProperty<double>("scalingExponent", j);
    o.mBackgroundTexture1 = cs::core::parseProperty<std::string>("backgroundTexture1", j);
    o.mBackgroundTexture2 = cs::core::parseProperty<std::string>("backgroundTexture2", j);

    auto b1 = j.at("backgroundColor1");
    for (int i = 0; i < 4; ++i) {
      o.mBackgroundColor1[i] = b1.at(i);
    }

    auto b2 = j.at("backgroundColor2");
    for (int i = 0; i < 4; ++i) {
      o.mBackgroundColor2[i] = b2.at(i);
    }

    o.mStarTexture = cs::core::parseProperty<std::string>("starTexture", j);

    o.mCacheFile        = cs::core::parseOptional<std::string>("cacheFile", j);
    o.mGaiaCatalog      = cs::core::parseOptional<std::string>("gaiaCatalog", j);
    o.mHipparcosCatalog = cs::core::parseOptional<std::string>("hipparcosCatalog", j);
    o.mTychoCatalog     = cs::core::parseOptional<std::string>("tychoCatalog", j);
    o.mTycho2Catalog    = cs::core::parseOptional<std::string>("tycho2Catalog", j);
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Plugin::Plugin()
    : mProperties(std::make_shared<Properties>()) {

  // Create default logger for this plugin.
  cs::utils::logger::init("csp-stars");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {

  spdlog::info("Loading plugin...");

  // init stars
  mPluginSettings = mAllSettings->mPlugins.at("csp-stars");

  std::map<Stars::CatalogType, std::string> catalogs;

  if (mPluginSettings.mGaiaCatalog) {
    catalogs[Stars::CatalogType::eGaia] = *mPluginSettings.mGaiaCatalog;
  }

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

  mStars = std::make_shared<Stars>(catalogs, mPluginSettings.mStarTexture, cacheFile);

  // set star settings
  auto& bg1 = mPluginSettings.mBackgroundColor1;
  auto& bg2 = mPluginSettings.mBackgroundColor2;

  mStars->setBackgroundTexture1(mPluginSettings.mBackgroundTexture1);
  mStars->setBackgroundTexture2(mPluginSettings.mBackgroundTexture2);

  mStars->setBackgroundColor1(VistaColor(bg1.r, bg1.g, bg1.b, bg1.a));
  mStars->setBackgroundColor2(VistaColor(bg2.r, bg2.g, bg2.b, bg2.a));

  mStars->setMinMagnitude((float)mPluginSettings.mMinMagnitude);
  mStars->setMaxMagnitude((float)mPluginSettings.mMaxMagnitude);

  mStars->setMinSize((float)mPluginSettings.mMinSize);
  mStars->setMaxSize((float)mPluginSettings.mMaxSize);

  mStars->setMinOpacity((float)mPluginSettings.mMinOpacity);
  mStars->setMaxOpacity((float)mPluginSettings.mMaxOpacity);

  mStars->setScalingExponent((float)mPluginSettings.mScalingExponent);
  mStars->setDraw3D(true);

  // add to scenegraph
  mStarsTransform = std::make_shared<cs::scene::CelestialAnchorNode>(
      mSceneGraph->GetRoot(), mSceneGraph->GetNodeBridge(), "", "Solar System Barycenter", "J2000");
  mSolarSystem->registerAnchor(mStarsTransform);

  mSceneGraph->GetRoot()->AddChild(mStarsTransform.get());

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mStarsTransform.get(), static_cast<int>(cs::utils::DrawOrder::eStars));

  mStarsNode = mSceneGraph->NewOpenGLNode(mStarsTransform.get(), mStars.get());

  mProperties->mEnabled.onChange().connect(
      [this](bool val) { this->mStarsNode->SetIsEnabled(val); });

  mGuiManager->addSettingsSectionToSideBarFromHTML(
      "Stars", "star", "../share/resources/gui/stars_settings.html");

  mGuiManager->getSideBar()->registerCallback<bool>(
      "set_enable_stars", ([this](bool value) { mProperties->mEnabled = value; }));

  mGuiManager->getSideBar()->registerCallback<bool>(
      "set_enable_stars_grid", ([this](bool value) { mProperties->mEnableCelestialGrid = value; }));

  mGuiManager->getSideBar()->registerCallback<bool>("set_enable_stars_figures",
      ([this](bool value) { mProperties->mEnableStarFigures = value; }));

  spdlog::info("Loading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  spdlog::info("Unloading plugin...");

  mSolarSystem->unregisterAnchor(mStarsTransform);
  mSceneGraph->GetRoot()->DisconnectChild(mStarsTransform.get());

  mGuiManager->getSideBar()->unregisterCallback("set_enable_stars");
  mGuiManager->getSideBar()->unregisterCallback("set_enable_stars_grid");
  mGuiManager->getSideBar()->unregisterCallback("set_enable_stars_figures");

  spdlog::info("Unloading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {
  float fIntensity = mGraphicsEngine->pApproximateSceneBrightness.get();

  mStars->setMaxSize((float)mPluginSettings.mMaxSize * fIntensity);
  mStars->setMinSize((float)mPluginSettings.mMinSize * fIntensity);
  mStars->setMaxOpacity((float)mPluginSettings.mMaxOpacity * fIntensity);
  mStars->setMinOpacity((float)mPluginSettings.mMinOpacity * fIntensity);
  mStars->setBackgroundColor1(VistaColor(
      0.5f, 0.8f, 1.f, 0.3f * fIntensity * (mProperties->mEnableCelestialGrid.get() ? 1.f : 0.f)));
  mStars->setBackgroundColor2(VistaColor(
      0.5f, 1.f, 0.8f, 0.3f * fIntensity * (mProperties->mEnableStarFigures.get() ? 1.f : 0.f)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::stars
