/* global IApi, CosmoScout */

/**
 * Stars Api
 */
class StarsApi extends IApi {
    /**
     * @inheritDoc
     */
    name = 'stars';
  
    /**
     * @inheritDoc
     */
    init() {
        CosmoScout.initSlider("set_star_magnitude", -10.0, 20.0, 0.1, [-5, 15]);
        CosmoScout.initSlider("set_star_size", 0.01, 1, 0.01, [0.05]);
        CosmoScout.initSlider("set_star_luminance_boost", 0.0, 20.0, 0.1, [0]);
    }
  }
  
  (() => {
    CosmoScout.init(StarsApi);
  })();