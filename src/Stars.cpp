////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Stars.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#endif

#include "../../../src/cs-graphics/TextureLoader.hpp"

#include <VistaInterProcComm/Connections/VistaByteBufferDeSerializer.h>
#include <VistaInterProcComm/Connections/VistaByteBufferSerializer.h>
#include <VistaKernel/GraphicsManager/VistaGeometryFactory.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaOGLExt/VistaBufferObject.h>
#include <VistaOGLExt/VistaGLSLShader.h>
#include <VistaOGLExt/VistaOGLUtils.h>
#include <VistaOGLExt/VistaTexture.h>
#include <VistaOGLExt/VistaVertexArrayObject.h>
#include <VistaTools/tinyXML/tinyxml.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <utility>

namespace csp::stars {

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool fromString(std::string const& v, T& out) {
  std::istringstream iss(v);
  iss >> out;
  return (iss.rdstate() & std::stringstream::failbit) == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::array<std::array<int, Stars::NUM_COLUMNS>, Stars::NUM_CATALOGS> Stars::cColumnMapping{
    std::array{34, 32, 11, 8, 9, 31}, // CatalogType::eHipparcos
    std::array{34, 32, 11, 8, 9, 31}, // CatalogType::eTycho
    std::array{19, 17, -1, 2, 3, 23}  // CatalogType::eTycho2
};

////////////////////////////////////////////////////////////////////////////////////////////////////

// Increase this if the cache format changed and is incompatible now. This will
// force a reload.
const int Stars::cCacheVersion = 3;

////////////////////////////////////////////////////////////////////////////////////////////////////

Stars::Stars(std::map<CatalogType, std::string> catalogs, std::string const& starTexture,
    const std::string& cacheFile)
    : mCatalogs(std::move(catalogs)) {
  init(starTexture, cacheFile);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setDrawMode(Stars::DrawMode value) {
  if (mDrawMode != value) {
    mShaderDirty = true;
    mDrawMode    = value;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Stars::DrawMode Stars::getDrawMode() const {
  return mDrawMode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setEnableHDR(bool value) {
  if (mEnableHDR != value) {
    mShaderDirty = true;
    mEnableHDR   = value;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setSolidAngle(float value) {
  mSolidAngle = value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getSolidAngle() const {
  return mSolidAngle;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::getEnableHDR() const {
  return mEnableHDR;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMinMagnitude(float value) {
  mMinMagnitude = value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMinMagnitude() const {
  return mMinMagnitude;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMaxMagnitude(float value) {
  mMaxMagnitude = value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMaxMagnitude() const {
  return mMaxMagnitude;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setLuminanceMultiplicator(float value) {
  mLuminanceMultiplicator = value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getLuminanceMultiplicator() const {
  return mLuminanceMultiplicator;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundColor1(const VistaColor& value) {
  mBackgroundColor1 = value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const VistaColor& Stars::getBackgroundColor1() const {
  return mBackgroundColor1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundColor2(const VistaColor& value) {
  mBackgroundColor2 = value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const VistaColor& Stars::getBackgroundColor2() const {
  return mBackgroundColor2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setStarTexture(const std::string& filename) {
  if (!filename.empty()) {
    mStarTexture = cs::graphics::TextureLoader::loadFromFile(filename);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundTexture1(const std::string& filename) {
  if (!filename.empty()) {
    mBackgroundTexture1 = cs::graphics::TextureLoader::loadFromFile(filename);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundTexture2(const std::string& filename) {
  if (!filename.empty()) {
    mBackgroundTexture2 = cs::graphics::TextureLoader::loadFromFile(filename);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::Do() {
  // save current state of the OpenGL state machine
  glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT);
  glDepthMask(GL_FALSE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  // get matrices
  std::array<GLfloat, 16> glMat{};
  glGetFloatv(GL_MODELVIEW_MATRIX, glMat.data());
  VistaTransformMatrix matModelView(glMat.data(), true);

  glGetFloatv(GL_PROJECTION_MATRIX, glMat.data());
  VistaTransformMatrix matProjection(glMat.data(), true);

  if (mShaderDirty) {
    std::string defines = "#version 330\n";

    if (mEnableHDR) {
      defines += "#define ENABLE_HDR\n";
    }

    if (mDrawMode == DrawMode::eSmoothPoint) {
      defines += "#define DRAWMODE_SMOOTH_POINT\n";
    } else if (mDrawMode == DrawMode::ePoint) {
      defines += "#define DRAWMODE_POINT\n";
    } else if (mDrawMode == DrawMode::eDisc) {
      defines += "#define DRAWMODE_DISC\n";
    } else if (mDrawMode == DrawMode::eSmoothDisc) {
      defines += "#define DRAWMODE_SMOOTH_DISC\n";
    } else if (mDrawMode == DrawMode::eSprite) {
      defines += "#define DRAWMODE_SPRITE\n";
    }

    mStarShader = VistaGLSLShader();
    if (mDrawMode == DrawMode::ePoint || mDrawMode == DrawMode::eSmoothPoint) {
      mStarShader.InitVertexShaderFromString(defines + cStarsSnippets + cStarsVertOnePixel);
      mStarShader.InitFragmentShaderFromString(defines + cStarsSnippets + cStarsFragOnePixel);
    } else {
      mStarShader.InitVertexShaderFromString(defines + cStarsSnippets + cStarsVert);
      mStarShader.InitGeometryShaderFromString(defines + cStarsSnippets + cStarsGeom);
      mStarShader.InitFragmentShaderFromString(defines + cStarsSnippets + cStarsFrag);
    }

    mStarShader.Link();

    mBackgroundShader = VistaGLSLShader();
    mBackgroundShader.InitVertexShaderFromString(defines + cBackgroundVert);
    mBackgroundShader.InitFragmentShaderFromString(defines + cBackgroundFrag);
    mBackgroundShader.Link();

    mShaderDirty = false;
  }

  // draw background
  if ((mBackgroundTexture1 && mBackgroundColor1[3] != 0.F) ||
      (mBackgroundTexture2 && mBackgroundColor2[3] != 0.F)) {
    mBackgroundVAO.Bind();
    mBackgroundShader.Bind();
    mBackgroundShader.SetUniform(mBackgroundShader.GetUniformLocation("iTexture"), 0);

    float backgroundIntensity = 1.0F;

    if (mEnableHDR) {
      backgroundIntensity = 0.001F * mLuminanceMultiplicator;
    }

    VistaTransformMatrix matMVNoTranslation = matModelView;

    // reduce jitter
    matMVNoTranslation[0][3] = 0.F;
    matMVNoTranslation[1][3] = 0.F;
    matMVNoTranslation[2][3] = 0.F;

    VistaTransformMatrix matMVP(matProjection * matMVNoTranslation);
    VistaTransformMatrix matInverseMVP(matMVP.GetInverted());
    VistaTransformMatrix matInverseMV(matMVNoTranslation.GetInverted());

    GLint loc = mBackgroundShader.GetUniformLocation("uInvMVP");
    glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseMVP.GetData());

    loc = mBackgroundShader.GetUniformLocation("uInvMV");
    glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseMV.GetData());

    if (mBackgroundTexture1 && mBackgroundColor1[3] != 0.F) {
      mBackgroundShader.SetUniform(mBackgroundShader.GetUniformLocation("cColor"),
          mBackgroundColor1[0], mBackgroundColor1[1], mBackgroundColor1[2],
          mBackgroundColor1[3] * backgroundIntensity);
      mBackgroundTexture1->Bind(GL_TEXTURE0);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      mBackgroundTexture1->Unbind(GL_TEXTURE0);
    }

    if (mBackgroundTexture2 && mBackgroundColor2[3] != 0.F) {
      mBackgroundShader.SetUniform(mBackgroundShader.GetUniformLocation("cColor"),
          mBackgroundColor2[0], mBackgroundColor2[1], mBackgroundColor2[2],
          mBackgroundColor2[3] * backgroundIntensity);
      mBackgroundTexture2->Bind(GL_TEXTURE0);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      mBackgroundTexture2->Unbind(GL_TEXTURE0);
    }

    mBackgroundShader.Release();
    mBackgroundVAO.Release();
  }

  // draw stars
  mStarVAO.Bind();
  mStarShader.Bind();

  if (mDrawMode == DrawMode::ePoint || mDrawMode == DrawMode::eSmoothPoint) {
    glPointSize(0.5F);
  }

  if (mDrawMode == DrawMode::eSmoothPoint) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
  } else {
    glDisable(GL_POINT_SMOOTH);
  }

  std::array<int, 4> viewport{};
  glGetIntegerv(GL_VIEWPORT, viewport.data());

  mStarShader.SetUniform(mStarShader.GetUniformLocation("uResolution"),
      static_cast<float>(viewport.at(2)), static_cast<float>(viewport.at(3)));

  mStarTexture->Bind(GL_TEXTURE0);
  mStarShader.SetUniform(mStarShader.GetUniformLocation("uStarTexture"), 0);
  mStarShader.SetUniform(mStarShader.GetUniformLocation("uMinMagnitude"), mMinMagnitude);
  mStarShader.SetUniform(mStarShader.GetUniformLocation("uMaxMagnitude"), mMaxMagnitude);
  mStarShader.SetUniform(mStarShader.GetUniformLocation("uSolidAngle"), mSolidAngle);
  mStarShader.SetUniform(
      mStarShader.GetUniformLocation("uLuminanceMultiplicator"), mLuminanceMultiplicator);

  VistaTransformMatrix matInverseMV(matModelView.GetInverted());
  VistaTransformMatrix matInverseP(matProjection.GetInverted());

  GLint loc = mStarShader.GetUniformLocation("uMatMV");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matModelView.GetData());

  loc = mStarShader.GetUniformLocation("uMatP");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matProjection.GetData());

  loc = mStarShader.GetUniformLocation("uInvMV");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseMV.GetData());

  loc = mStarShader.GetUniformLocation("uInvP");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseP.GetData());

  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(mStars.size()));

  mStarTexture->Unbind(GL_TEXTURE0);

  mStarShader.Release();
  mStarVAO.Release();

  glDepthMask(GL_TRUE);
  glPopAttrib();

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::GetBoundingBox(VistaBoundingBox& oBoundingBox) {
  float      min(std::numeric_limits<float>::min());
  float      max(std::numeric_limits<float>::max());
  std::array fMin{min, min, min};
  std::array fMax{max, max, max};

  oBoundingBox.SetBounds(fMin.data(), fMax.data());

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::init(const std::string& sStarTextureFile, const std::string& sCacheFile) {
  // spectral colors from B-V index -0.4 to 2.0 in steps of 0.05
  // values from  http://www.vendian.org/mncharity/dir3/starcolor/details.html
  mSpectralColors.emplace_back(VistaColor(0x9bb2ff));
  mSpectralColors.emplace_back(VistaColor(0x9eb5ff));
  mSpectralColors.emplace_back(VistaColor(0xa3b9ff));
  mSpectralColors.emplace_back(VistaColor(0xaabfff));
  mSpectralColors.emplace_back(VistaColor(0xb2c5ff));
  mSpectralColors.emplace_back(VistaColor(0xbbccff));
  mSpectralColors.emplace_back(VistaColor(0xc4d2ff));
  mSpectralColors.emplace_back(VistaColor(0xccd8ff));
  mSpectralColors.emplace_back(VistaColor(0xd3ddff));
  mSpectralColors.emplace_back(VistaColor(0xdae2ff));
  mSpectralColors.emplace_back(VistaColor(0xdfe5ff));
  mSpectralColors.emplace_back(VistaColor(0xe4e9ff));
  mSpectralColors.emplace_back(VistaColor(0xe9ecff));
  mSpectralColors.emplace_back(VistaColor(0xeeefff));
  mSpectralColors.emplace_back(VistaColor(0xf3f2ff));
  mSpectralColors.emplace_back(VistaColor(0xf8f6ff));
  mSpectralColors.emplace_back(VistaColor(0xfef9ff));
  mSpectralColors.emplace_back(VistaColor(0xfff9fb));
  mSpectralColors.emplace_back(VistaColor(0xfff7f5));
  mSpectralColors.emplace_back(VistaColor(0xfff5ef));
  mSpectralColors.emplace_back(VistaColor(0xfff3ea));
  mSpectralColors.emplace_back(VistaColor(0xfff1e5));
  mSpectralColors.emplace_back(VistaColor(0xffefe0));
  mSpectralColors.emplace_back(VistaColor(0xffeddb));
  mSpectralColors.emplace_back(VistaColor(0xffebd6));
  mSpectralColors.emplace_back(VistaColor(0xffe8ce));
  mSpectralColors.emplace_back(VistaColor(0xffe6ca));
  mSpectralColors.emplace_back(VistaColor(0xffe5c6));
  mSpectralColors.emplace_back(VistaColor(0xffe3c3));
  mSpectralColors.emplace_back(VistaColor(0xffe2bf));
  mSpectralColors.emplace_back(VistaColor(0xffe0bb));
  mSpectralColors.emplace_back(VistaColor(0xffdfb8));
  mSpectralColors.emplace_back(VistaColor(0xffddb4));
  mSpectralColors.emplace_back(VistaColor(0xffdbb0));
  mSpectralColors.emplace_back(VistaColor(0xffdaad));
  mSpectralColors.emplace_back(VistaColor(0xffd8a9));
  mSpectralColors.emplace_back(VistaColor(0xffd6a5));
  mSpectralColors.emplace_back(VistaColor(0xffd29c));
  mSpectralColors.emplace_back(VistaColor(0xffd096));
  mSpectralColors.emplace_back(VistaColor(0xffcc8f));
  mSpectralColors.emplace_back(VistaColor(0xffc885));
  mSpectralColors.emplace_back(VistaColor(0xffc178));
  mSpectralColors.emplace_back(VistaColor(0xffb765));
  mSpectralColors.emplace_back(VistaColor(0xffa94b));
  mSpectralColors.emplace_back(VistaColor(0xff9523));
  mSpectralColors.emplace_back(VistaColor(0xff7b00));
  mSpectralColors.emplace_back(VistaColor(0xff5200));

  // read star catalog -------------------------------------------------------
  if (!readStarCache(sCacheFile)) {
    std::map<CatalogType, std::string>::const_iterator it;

    it = mCatalogs.find(CatalogType::eHipparcos);
    if (it != mCatalogs.end()) {
      readStarsFromCatalog(it->first, it->second);
    }

    it = mCatalogs.find(CatalogType::eTycho);
    if (it != mCatalogs.end()) {
      readStarsFromCatalog(it->first, it->second);
    }

    it = mCatalogs.find(CatalogType::eTycho2);
    if (it != mCatalogs.end()) {
      // do not load tycho and tycho 2
      if (mCatalogs.find(CatalogType::eTycho) == mCatalogs.end()) {
        readStarsFromCatalog(it->first, it->second);
      } else {
        spdlog::warn("Failed to load Tycho2 catalog: Tycho already loaded!");
      }
    }

    if (!mStars.empty()) {
      writeStarCache(sCacheFile);
    } else {
      spdlog::warn("Loaded no stars! Stars will not work properly.");
    }
  }

  // create texture ----------------------------------------------------------
  setStarTexture(sStarTextureFile);

  // create buffers ----------------------------------------------------------
  buildStarVAO();
  buildBackgroundVAO();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::readStarsFromCatalog(CatalogType type, const std::string& filename) {
  bool success = false;
  spdlog::info("Reading star catalog '{}'.", filename);

  std::ifstream file;

  try {
    file.open(filename.c_str(), std::ifstream::in);
  } catch (std::exception& e) {
    spdlog::error("Failed to open catalog file '{}': {}", filename, e.what());
  }

  if (file.is_open()) {
    int  lineCount = 0;
    bool loadHipparcos(mCatalogs.find(CatalogType::eHipparcos) != mCatalogs.end());

    // read line by line
    while (!file.eof()) {
      // get line
      ++lineCount;
      std::string line;
      getline(file, line);

      // parse line:
      // separate complete items consisting of "val0|val1|...|valN|" into vector of value
      // strings
      std::stringstream        stream(line);
      std::string              item;
      std::vector<std::string> items;

      while (getline(stream, item, '|')) {
        items.emplace_back(item);
      }

      // convert value strings to int/double/float and save in star data structure
      // expecting Hipparcos or Tycho-1 catalog and more than 12 columns
      if (items.size() > 12) {
        // skip if part of hipparcos catalogue
        int tmp{};
        if (type != CatalogType::eHipparcos && loadHipparcos &&
            fromString<int>(items[cColumnMapping.at(cs::utils::enumCast(
                                type))[cs::utils::enumCast(CatalogColumn::eHipp)]],
                tmp)) {
          continue;
        }

        // store star data
        bool successStoreData(true);

        Star star{};
        successStoreData &= fromString<float>(
            items[cColumnMapping.at(
                cs::utils::enumCast(type))[cs::utils::enumCast(CatalogColumn::eVmag)]],
            star.mVMagnitude);
        successStoreData &= fromString<float>(
            items[cColumnMapping.at(
                cs::utils::enumCast(type))[cs::utils::enumCast(CatalogColumn::eBmag)]],
            star.mBMagnitude);
        successStoreData &= fromString<float>(
            items[cColumnMapping.at(
                cs::utils::enumCast(type))[cs::utils::enumCast(CatalogColumn::eRect)]],
            star.mAscension);
        successStoreData &= fromString<float>(
            items[cColumnMapping.at(
                cs::utils::enumCast(type))[cs::utils::enumCast(CatalogColumn::eDecl)]],
            star.mDeclination);

        if (cColumnMapping.at(
                cs::utils::enumCast(type))[cs::utils::enumCast(CatalogColumn::ePara)] > 0) {
          if (!fromString<float>(items[cColumnMapping.at(cs::utils::enumCast(
                                     type))[cs::utils::enumCast(CatalogColumn::ePara)]],
                  star.mParallax)) {
            star.mParallax = 0;
          }
        } else {
          star.mParallax = 0;
        }

        if (successStoreData) {
          star.mAscension   = (360.F + 90.F - star.mAscension) / 180.F * Vista::Pi;
          star.mDeclination = star.mDeclination / 180.F * Vista::Pi;

          mStars.emplace_back(star);
        }
      }

      // print progress status
      if (mStars.size() % 10000 == 0) {
        spdlog::info("Read {} stars so far...", mStars.size());
      }
    }
    file.close();
    success = true;

    spdlog::info("Read a total of {} stars.", mStars.size());
  } else {
    spdlog::error("Failed to load stars: Cannot open catalog file '{}'!", filename);
  }

  return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::writeStarCache(const std::string& sCacheFile) const {
  VistaType::uint32 catalogs = 0;
  for (auto const& mCatalog : mCatalogs) {
    catalogs += static_cast<uint32_t>(std::pow(2, static_cast<int>(mCatalog.first)));
  }

  VistaByteBufferSerializer serializer;
  serializer.WriteInt32(
      static_cast<VistaType::uint32>(cCacheVersion)); // cache format version number
  serializer.WriteInt32(catalogs);                    // cache format version number
  serializer.WriteInt32(static_cast<VistaType::uint32>(
      mStars.size())); // write number of stars to front of byte stream

  for (const auto& mStar : mStars) {
    // serialize star data into byte stream
    serializer.WriteFloat32(mStar.mVMagnitude);
    serializer.WriteFloat32(mStar.mBMagnitude);
    serializer.WriteFloat32(mStar.mAscension);
    serializer.WriteFloat32(mStar.mDeclination);
    serializer.WriteFloat32(mStar.mParallax);
  }

  // open file
  std::ofstream file;
  file.open(sCacheFile.c_str(), std::ios::out | std::ios::binary);
  if (file.is_open()) {
    // write serialized star data
    spdlog::info("Writing {} stars ({} bytes) into '{}'.", mStars.size(),
        serializer.GetBufferSize(), sCacheFile);
    file.write(reinterpret_cast<const char*>(serializer.GetBuffer()), serializer.GetBufferSize());
    file.close();
  } else {
    spdlog::error(
        "Failed to write binary star data: Cannot open file '{}' for writing!", sCacheFile);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::readStarCache(const std::string& sCacheFile) {
  bool success = false;

  // open file
  std::ifstream file;
  file.open(sCacheFile.c_str(),
      std::ios::in | std::ios::binary | std::ios::ate); // ate = set read pointer to end
  if (file.is_open()) {
    // read binary data from file
    int size = static_cast<int>(file.tellg()); // get position of read pointer
    std::vector<VistaType::byte> data(size);
    file.seekg(0, std::ios::beg); // set read pointer to the beginning of file stream
    file.read(reinterpret_cast<char*>(&data[0]), size); // read file stream into char array 'data'
    file.close();

    // de-serialize byte stream
    VistaType::uint32 cacheVersion = 0;
    VistaType::uint32 catalogs     = 0;
    VistaType::uint32 numStars     = 0;

    VistaByteBufferDeSerializer deserializer;
    deserializer.SetBuffer(&data[0], size); // prepare for de-serialization
    deserializer.ReadInt32(cacheVersion);   // read cache format version number
    deserializer.ReadInt32(catalogs);       // read which catalogs were loaded
    deserializer.ReadInt32(numStars);       // read number of stars from front of byte stream

    if (cacheVersion != cCacheVersion) {
      return false;
    }

    VistaType::uint32 catalogsToLoad = 0;
    for (const auto& mCatalog : mCatalogs) {
      catalogsToLoad += static_cast<uint32_t>(std::pow(2, static_cast<int>(mCatalog.first)));
    }

    if (catalogs != catalogsToLoad) {
      return false;
    }

    for (unsigned int num = 0; num < numStars; ++num) {
      Star star{};
      deserializer.ReadFloat32(star.mVMagnitude);
      deserializer.ReadFloat32(star.mBMagnitude);
      deserializer.ReadFloat32(star.mAscension);
      deserializer.ReadFloat32(star.mDeclination);
      deserializer.ReadFloat32(star.mParallax);

      mStars.emplace_back(star);

      // print progress status
      if (mStars.size() % 100000 == 0) {
        spdlog::info("Read {} stars so far...", mStars.size());
      }
    }

    success = true;

    spdlog::info("Read a total of {} stars.", mStars.size());
  }

  return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::buildStarVAO() {
  int                c(0);
  const int          iElementCount(7);
  std::vector<float> data(iElementCount * mStars.size());

  for (auto it = mStars.begin(); it != mStars.end(); ++it, c += iElementCount) {
    // use B and V magnitude to retrieve the according color
    const float minIdx(-0.4F);
    const float maxIdx(2.0F);
    const float step(0.05F);
    float       bvIndex = std::min(maxIdx, std::max(minIdx, it->mBMagnitude - it->mVMagnitude));
    float       normalizedIndex = (bvIndex - minIdx) / (maxIdx - minIdx) / step + 0.5F;
    VistaColor  color           = mSpectralColors[static_cast<int>(normalizedIndex)];

    // distance in parsec --- some have parallax of zero; assume a
    // large distance in those cases
    float fDist = 100000.F;

    if (it->mParallax > 0.F) {
      fDist = 1000.F / it->mParallax;
    }

    data[c]     = it->mDeclination;
    data[c + 1] = it->mAscension;
    data[c + 2] = fDist;
    data[c + 3] = color.GetRed();
    data[c + 4] = color.GetGreen();
    data[c + 5] = color.GetBlue();
    data[c + 6] = it->mVMagnitude - 5.F * std::log10(fDist / 10.F);
  }

  mStarVBO.Bind(GL_ARRAY_BUFFER);
  mStarVBO.BufferData(iElementCount * mStars.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
  mStarVBO.Release();

  // star positions
  mStarVAO.EnableAttributeArray(0);
  mStarVAO.SpecifyAttributeArrayFloat(
      0, 2, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 0, &mStarVBO);

  // star distances
  mStarVAO.EnableAttributeArray(1);
  mStarVAO.SpecifyAttributeArrayFloat(
      1, 1, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 2 * sizeof(float), &mStarVBO);

  // color
  mStarVAO.EnableAttributeArray(2);
  mStarVAO.SpecifyAttributeArrayFloat(
      2, 3, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 3 * sizeof(float), &mStarVBO);

  // magnitude
  mStarVAO.EnableAttributeArray(3);
  mStarVAO.SpecifyAttributeArrayFloat(
      3, 1, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 6 * sizeof(float), &mStarVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::buildBackgroundVAO() {
  std::vector<float> data(8);
  data[0] = -1;
  data[1] = 1;
  data[2] = 1;
  data[3] = 1;
  data[4] = -1;
  data[5] = -1;
  data[6] = 1;
  data[7] = -1;

  mBackgroundVBO.Bind(GL_ARRAY_BUFFER);
  mBackgroundVBO.BufferData(data.size() * sizeof(float), &(data[0]), GL_STATIC_DRAW);
  mBackgroundVBO.Release();

  // positions
  mBackgroundVAO.EnableAttributeArray(0);
  mBackgroundVAO.SpecifyAttributeArrayFloat(
      0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0, &mBackgroundVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::stars
