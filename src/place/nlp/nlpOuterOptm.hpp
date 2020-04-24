/**
 * @file nlpOuterOptm.hpp
 * @brief The non-lnear programming outer-problem optimization
 * @author Keren Zhu
 * @date 04/09/2020
 */

#pragma once

#include "global/global.h"
#include "nlpTypes.hpp"
#include "place/different.h"

PROJECT_NAMESPACE_BEGIN

namespace nlp 
{
    namespace outer_stop_condition
    {
        /* Stop outer problem condition */
        template<typename T>
        struct stop_condition_trait 
        {
            // static T construct(NlpType &)
            // static IntType stopPlaceCondition(T&, NlpType &)
        };

        /// @brief stop condition with number of iterations
        template<IntType MaxIter=10>
        struct stop_after_num_outer_iterations
        {
            static constexpr IntType maxIter = MaxIter;
            IntType curIter = 0;
        };
        
        template<IntType MaxIter>
        struct stop_condition_trait<stop_after_num_outer_iterations<MaxIter>>
        {
            template<typename NlpType>
            static stop_after_num_outer_iterations<MaxIter> construct(NlpType &) { return stop_after_num_outer_iterations<MaxIter>(); }

            template<typename NlpType>
            static void init(NlpType &, stop_after_num_outer_iterations<MaxIter> &) {}

            static void clear(stop_after_num_outer_iterations<MaxIter> &) {}
            
            template<typename NlpType>
            static BoolType stopPlaceCondition(NlpType &, stop_after_num_outer_iterations<MaxIter> &stop)
            {
                if (stop.curIter >= stop.maxIter)
                {
                    stop.curIter = 0;
                    return 1;
                }
                ++stop.curIter;
                return 0;
            }
        };

        /// @brief stop after violating is small enough
        struct stop_after_violate_small
        {
            static constexpr RealType overlapRatio = 0.01; ///< with respect to total cell area
            static constexpr RealType outOfBoundaryRatio = 0.05; ///< with respect to boundary
            static constexpr RealType asymRatio = 0.05; ///< with respect to sqrt(total cell area)
        };

        template<>
        struct stop_condition_trait<stop_after_violate_small>
        {
            typedef stop_after_violate_small stop_type;

            template<typename NlpType>
            static stop_type construct(NlpType &) { return stop_type(); }

            template<typename NlpType>
            static void init(NlpType &, stop_type &) {}

            static void clear(stop_type &) {}
            
            template<typename NlpType>
            static BoolType stopPlaceCondition(NlpType &n, stop_type &stop)
            {
                using CoordType = typename NlpType::nlp_coordinate_type;
                // check whether overlapping is small than threshold
                CoordType ovlArea = 0;
                const CoordType ovlThreshold = stop.overlapRatio * n._totalCellArea;
                for (auto &op : n._ovlOps)
                {
                    ovlArea += diff::place_overlap_trait<typename NlpType::nlp_ovl_type>::overlapArea(op);
                    if (ovlArea > ovlThreshold)
                    {
                        return false;
                    }
                }
                // Check whether out of boundary is smaller than threshold
                CoordType oobArea = 0;
                const CoordType oobThreshold = stop.outOfBoundaryRatio * n._boundary.area();
                for (auto &op : n._oobOps)
                {
                    oobArea +=  diff::place_out_of_boundary_trait<typename NlpType::nlp_oob_type>::oobArea(op);
                    if (oobArea > oobThreshold)
                    {
                        return false;
                    }
                }
                // Check whether asymmetry distance is smaller than threshold
                CoordType asymDist = 0;
                const CoordType asymThreshold = stop.asymRatio * std::sqrt(n._totalCellArea);
                for (auto & op : n._asymOps)
                {
                    asymDist += diff::place_asym_trait<typename NlpType::nlp_asym_type>::asymDistanceNormalized(op);
                    if (asymDist > asymThreshold)
                    {
                        return false;
                    }
                }
                DBG("ovl area %f target %f \n oob area %f target %f \n asym dist %f target %f \n",  ovlArea, ovlThreshold, oobArea, oobThreshold, asymDist, asymThreshold);
                return true;
            }
        };
        /// @brief a convenient wrapper for combining different types of stop_condition condition. the list in the template will be check one by one and return converge if any of them say so
        template<typename stop_condition_type, typename... others>
        struct stop_condition_list 
        {
            typedef stop_condition_list<others...> base_type;
            stop_condition_type  _stop;
            stop_condition_list<others...> _list;
        };

        template<typename stop_condition_type>
        struct stop_condition_list<stop_condition_type>
        {
            stop_condition_type _stop;
        };

        template<typename stop_condition_type, typename... others>
        struct stop_condition_trait<stop_condition_list<stop_condition_type, others...>>
        {
            typedef stop_condition_list<stop_condition_type, others...> list_type;
            typedef typename stop_condition_list<stop_condition_type, others...>::base_type base_type;

            static void clear(list_type &c)
            {
                stop_condition_trait<stop_condition_type>::clear(c._stop);
                stop_condition_trait<base_type>::clear(c._list);
            }

            template<typename nlp_type>
            static list_type construct(nlp_type &n)
            {
                list_type list;
                list._stop = std::move(stop_condition_trait<stop_condition_type>::construct(n));
                list._list = std::move(stop_condition_trait<base_type>::construct(n));
                return list;
            }

            template<typename nlp_type>
            static BoolType stopPlaceCondition(nlp_type &n,  list_type &c) 
            {
                BoolType stop = false;
                if (stop_condition_trait<stop_condition_type>::stopPlaceCondition(n, c._stop))
                {
                    stop = true;
                }
                if (stop_condition_trait<base_type>::stopPlaceCondition(n, c._list))
                {
                    stop = true;
                }
                if (stop)
                {
                    stop_condition_trait<stop_condition_type>::clear(c._stop);
                }
                return stop;
            }
        };


        template<typename stop_condition_type>
        struct stop_condition_trait<stop_condition_list<stop_condition_type>>
        {
            typedef stop_condition_list<stop_condition_type> list_type;
            static void clear(stop_condition_list<stop_condition_type> &c)
            {
                stop_condition_trait<stop_condition_type>::clear(c._stop);
            }


            template<typename nlp_type>
            static list_type construct(nlp_type &n)
            {
                list_type list;
                list._stop = std::move(stop_condition_trait<stop_condition_type>::construct(n));
                return list;
            }

            template<typename nlp_type>
            static BoolType stopPlaceCondition(nlp_type &n, stop_condition_list<stop_condition_type>&c) 
            {
                if (stop_condition_trait<stop_condition_type>::stopPlaceCondition(n, c._stop))
                {
                    stop_condition_trait<stop_condition_type>::clear(c._stop);
                    return true;
                }
                return false;
            }
        };

    } // namespace outer_stop_condition

    namespace outer_multiplier
    {
        /// @brief the multiplier is categoried by types
        template<typename mult_type>
        struct is_mult_type_dependent_diff : std::false_type {};

        namespace init
        {
            template<typename T>
            struct multiplier_init_trait {};

            struct hard_code_init {};

            template<>
            struct multiplier_init_trait<hard_code_init>
            {
                template<typename nlp_type, typename mult_type>
                static void init(nlp_type &, mult_type& mult)
                {
                    mult._constMults.at(0) = 16; // hpwl
                    mult._constMults.at(1) = 16; // cos
                    mult._variedMults.at(0) = 1; // ovl
                    mult._variedMults.at(1) = 1; // oob
                    mult._variedMults.at(2) = 1; // asym
                }

            };

            /// @brief match by gradient norm
            struct init_by_matching_gradient_norm 
            {
                static constexpr RealType penaltyRatioToObj = 1.0; ///< The ratio of targeting penalty 
                static constexpr RealType small = 0.01;
            };

            
            template<>
            struct multiplier_init_trait<init_by_matching_gradient_norm>
            {
                typedef init_by_matching_gradient_norm init_type;
                template<typename nlp_type, typename mult_type, std::enable_if_t<nlp::is_first_order_diff<nlp_type>::value, void>* = nullptr>
                static void init(nlp_type &nlp, mult_type& mult)
                {
                    mult._constMults.at(0) = 1.0; // hpwl
                    const auto hpwlMult = mult._constMults.at(0);
                    const auto hpwlNorm = nlp._gradHpwl.norm();
                    const auto hpwlMultNorm = hpwlMult * hpwlNorm;
                    const auto hpwlMultNormPenaltyRatio = hpwlMultNorm * init_type::penaltyRatioToObj;
                    const auto cosNorm = nlp._gradCos.norm();
                    const auto ovlNorm = nlp._gradOvl.norm();
                    const auto oobNorm = nlp._gradOob.norm();
                    const auto asymNorm = nlp._gradAsym.norm();
                    const auto maxPenaltyNorm = ovlNorm;
                    // Make a threshold on by referencing hpwl to determine whether one is small
                    const auto small  = init_type::small * hpwlNorm;

                    // Fix corner case that may happen when the placement is very small
                    if (hpwlNorm < REAL_TYPE_TOL)
                    {
                        mult._constMults.resize(2, 1);
                        mult._variedMults.resize(3, 1);
                        WRN("Ideaplace: NLP global placement: init multipliers: wire length  gradient norm is very small %f!, ", hpwlNorm);
                        return;
                    }
                    // match gradient norm for signal path
                    if (cosNorm > small)
                    {
                        mult._constMults.at(1) = hpwlMultNorm / cosNorm;
                    }
                    else
                    {
                        mult._constMults.at(1) = hpwlMult;
                    }
                    // overlap
                    if (ovlNorm > small)
                    {
                        mult._variedMults.at(0) = hpwlMultNormPenaltyRatio / ovlNorm;
                    }
                    else
                    {
                        mult._variedMults.at(0) = hpwlMultNormPenaltyRatio / maxPenaltyNorm;
                    }
                    // out of boundary
                    // Since we know oob is small at beginning, and it will take effect after a few iterations. Therefore it would be better to set it to resonable range first
                    //mult._variedMults.at(1) = hpwlMultNormPenaltyRatio;
                    if (oobNorm > small)
                    {
                        mult._variedMults.at(1) = hpwlMultNormPenaltyRatio / oobNorm;
                    }
                    else
                    {
                        mult._variedMults.at(1) = hpwlMultNormPenaltyRatio / maxPenaltyNorm;
                    }
                    // asym
                    if (asymNorm > small)
                    {
                        mult._variedMults.at(2) = hpwlMultNormPenaltyRatio / asymNorm;
                    }
                    else
                    {
                        mult._variedMults.at(2) = hpwlMultNormPenaltyRatio / maxPenaltyNorm;
                    }
                    DBG("init mult: hpwl %f cos %f \n",
                            mult._constMults[0], mult._constMults[1]);
                    DBG("init mult: ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                }
            };
        }; // namespace init
        namespace update
        {
            template<typename T>
            struct multiplier_update_trait {};

            /// @brief increase the total amounts of penalty of by a constant
            struct shared_constant_increase_penalty
            {
                static constexpr RealType penalty  = 20;
            };

            template<>
            struct multiplier_update_trait<shared_constant_increase_penalty>
            {
                typedef shared_constant_increase_penalty update_type;

                template<typename nlp_type, typename mult_type, std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static update_type construct(nlp_type &, mult_type&) { return update_type(); }

                template<typename nlp_type, typename mult_type,  std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static void update(nlp_type &nlp, mult_type &mult, update_type &update)
                {
                    nlp._wrapObjAllTask.run();
                    const auto rawOvl = nlp._objOvl / mult._variedMults.at(0);
                    const auto rawOob = nlp._objOob / mult._variedMults.at(1);
                    const auto rawAsym = nlp._objAsym / mult._variedMults.at(2);
                    const auto fViolate = rawOvl + rawOob + rawAsym;
                    DBG("update mult: raw ovl %f oob %f asym %f total %f \n", rawOvl, rawOob, rawAsym, fViolate);
                    DBG("update mult:  before ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                    mult._variedMults.at(0) += update.penalty * (rawOvl / fViolate);
                    mult._variedMults.at(1) += update.penalty * (rawOob / fViolate);
                    mult._variedMults.at(2) += update.penalty * (rawAsym / fViolate);
                    DBG("update mult: afterafter  ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                }
            };

            /// @brief direct subgradient
            struct direct_subgradient
            {
                static constexpr RealType stepSize = 0.01;
            };

            template<>
            struct multiplier_update_trait<direct_subgradient>
            {
                typedef direct_subgradient update_type;

                template<typename nlp_type, typename mult_type, std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static update_type construct(nlp_type &, mult_type&) { return update_type(); }

                template<typename nlp_type, typename mult_type, std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static void init(nlp_type &, mult_type&, update_type &) 
                { 
                }

                template<typename nlp_type, typename mult_type,  std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static void update(nlp_type &nlp, mult_type &mult, update_type &update)
                {
                    nlp._wrapObjAllTask.run();
                    const auto rawOvl = nlp._objOvl / mult._variedMults.at(0);
                    const auto rawOob = nlp._objOob / mult._variedMults.at(1);
                    const auto rawAsym = nlp._objAsym / mult._variedMults.at(2);
                    DBG("update mult: raw ovl %f oob %f asym %f total %f \n", rawOvl, rawOob, rawAsym);
                    DBG("update mult:  before ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                    mult._variedMults.at(0) += update.stepSize * (rawOvl );
                    mult._variedMults.at(1) += update.stepSize * (rawOob );
                    mult._variedMults.at(2) += update.stepSize * (rawAsym );
                    DBG("update mult: afterafter  ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                }
            };

            /// @breif subgradient normalize by values in iter 0
            template<typename nlp_numerical_type>
            struct subgradient_normalized_by_init
            {
                static constexpr nlp_numerical_type stepSize = 10;
                std::vector<nlp_numerical_type> normalizeFactor;
            };

            template<typename nlp_numerical_type>
            struct multiplier_update_trait<subgradient_normalized_by_init<nlp_numerical_type>>
            {
                typedef subgradient_normalized_by_init<nlp_numerical_type> update_type;

                template<typename nlp_type, typename mult_type, std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static update_type construct(nlp_type &, mult_type&) { return update_type(); }

                template<typename nlp_type, typename mult_type, std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static void init(nlp_type &nlp, mult_type &mult, update_type &update) 
                { 
                    update.normalizeFactor.resize(3);
                    update.normalizeFactor.at(0) = mult._variedMults.at(0) / nlp._objOvl;
                    update.normalizeFactor.at(1) = 1;// mult._variedMults.at(1) / nlp._objOob;
                    update.normalizeFactor.at(2) = mult._variedMults.at(2) / nlp._objAsym;
                    //update.normalizeFactor.at(0) = 1 / nlp._objOvl;
                    //update.normalizeFactor.at(1) = 1;// / nlp._objOob;
                    //update.normalizeFactor.at(2) = 1 / nlp._objAsym;
                }

                template<typename nlp_type, typename mult_type,  std::enable_if_t<is_mult_type_dependent_diff<mult_type>::value, void>* = nullptr>
                static void update(nlp_type &nlp, mult_type &mult, update_type &update)
                {
                    nlp._wrapObjAllTask.run();
                    const auto rawOvl = nlp._objOvl / mult._variedMults.at(0);
                    const auto rawOob = nlp._objOob / mult._variedMults.at(1);
                    const auto rawAsym = nlp._objAsym / mult._variedMults.at(2);
                    const auto normalizedOvl = rawOvl * update.normalizeFactor.at(0);
                    const auto normalizedOob = rawOob * update.normalizeFactor.at(1);
                    const auto normalizedAsym = rawAsym  * update.normalizeFactor.at(2);
                    DBG("update mult: raw ovl %f oob %f asym %f total %f \n", normalizedOvl, normalizedOob, normalizedAsym);
                    DBG("update mult:  before ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                    mult._variedMults.at(0) += update.stepSize * (normalizedOvl );
                    mult._variedMults.at(1) += update.stepSize * (normalizedOob );
                    mult._variedMults.at(2) += update.stepSize * (normalizedAsym );
                    DBG("update mult: afterafter  ovl %f oob %f asym %f \n",
                            mult._variedMults[0], mult._variedMults[1], mult._variedMults[2]);
                }
            };


        } // namespace update
        template<typename T> 
        struct multiplier_trait {};

        template<typename nlp_numerical_type, typename init_type, typename update_type>
        struct mult_const_hpwl_cos_and_penalty_by_type
        {
            typedef init_type mult_init_type;
            typedef update_type mult_update_type;
            std::vector<nlp_numerical_type> _constMults; ///< constant mults
            std::vector<nlp_numerical_type> _variedMults; ///< varied penalty multipliers
            update_type update;
        };

        template<typename nlp_numerical_type, typename init_type, typename update_type>
        struct is_mult_type_dependent_diff<mult_const_hpwl_cos_and_penalty_by_type<nlp_numerical_type, init_type, update_type>> : std::true_type {};

        template<typename nlp_numerical_type, typename init_type, typename update_type>
        struct multiplier_trait<mult_const_hpwl_cos_and_penalty_by_type<nlp_numerical_type, init_type, update_type>>
        {
            typedef mult_const_hpwl_cos_and_penalty_by_type<nlp_numerical_type, init_type, update_type> mult_type;

            template<typename nlp_type>
            static mult_type construct(nlp_type &nlp)
            {
                mult_type mult;
                mult._constMults.resize(2, 0.0);
                mult._variedMults.resize(3, 0.0);
                mult.update = update::multiplier_update_trait<update_type>::construct(nlp, mult);
                return mult;
            }

            template<typename nlp_type>
            static void init(nlp_type &nlp, mult_type &mult)
            {
                init::multiplier_init_trait<init_type>::init(nlp, mult);
                update::multiplier_update_trait<update_type>::init(nlp, mult, mult.update);
                for (auto &op : nlp._hpwlOps) { op._getLambdaFunc = [&](){ return mult._constMults[0]; }; }
                for (auto &op : nlp._cosOps) { op._getLambdaFunc = [&](){ return mult._constMults[1]; }; }
                for (auto &op : nlp._ovlOps) { op._getLambdaFunc = [&](){ return mult._variedMults[0]; }; }
                for (auto &op : nlp._oobOps) { op._getLambdaFunc = [&](){ return mult._variedMults[1]; }; }
                for (auto &op : nlp._asymOps) { op._getLambdaFunc = [&](){ return mult._variedMults[2]; }; }
            }

            template<typename nlp_type>
            static void update(nlp_type &nlp, mult_type &mult)
            {
                update::multiplier_update_trait<update_type>::update(nlp, mult, mult.update);
            }

            template<typename nlp_type>
            static void recordRaw(nlp_type &nlp, mult_type &mult)
            {
                nlp._objHpwlRaw = nlp._objHpwl / mult._constMults[0];
                nlp._objCosRaw = nlp._objCos / mult._constMults[1];
                nlp._objOvlRaw = nlp._objOvl / mult._variedMults[0];
                nlp._objOobRaw = nlp._objOob / mult._variedMults[1];
                nlp._objAsymRaw = nlp._objAsym / mult._variedMults[2];
            }

        };
    } // namespace outer_multiplier

    /// @brief alpha 
    namespace alpha
    {
        namespace update
        {
            template<typename T>
            struct alpha_update_trait {};

        } //namespace update

        template<typename T>
        struct alpha_trait {};

        template<typename nlp_numerical_type>
        struct alpha_hpwl_ovl_oob
        {
            std::vector<nlp_numerical_type> _alpha;
        };

        template<typename nlp_numerical_type>
        struct alpha_trait<alpha_hpwl_ovl_oob<nlp_numerical_type>>
        {
            typedef alpha_hpwl_ovl_oob<nlp_numerical_type> alpha_type;

            template<typename nlp_type>
            static alpha_type construct(nlp_type &)
            {
                alpha_type alpha;
                alpha._alpha.resize(3, 1.0);
                return alpha;
            }

            template<typename nlp_type>
            static void init(nlp_type &nlp, alpha_type &alpha)
            {
                for (auto & op : nlp._hpwlOps)
                {
                    op.setGetAlphaFunc([&](){ return alpha._alpha[0]; });
                }
                for (auto & op : nlp._ovlOps)
                {
                    op.setGetAlphaFunc([&](){ return alpha._alpha[1]; });
                }
                for (auto & op : nlp._oobOps)
                {
                    op.setGetAlphaFunc([&](){ return alpha._alpha[2]; });
                }
            }

        };

        namespace update
        {
            /// @breif update the alpha that mapping objective function to alpha, from [0, init_obj] -> [min, max]
            /// @tparam the index of which alpha to update
            template<typename nlp_numerical_type, IndexType alphaIdx>
            struct exponential_by_obj
            {
                static constexpr nlp_numerical_type alphaMax = 1.5;
                static constexpr nlp_numerical_type alphaMin = 0.4;
                static constexpr nlp_numerical_type alphaMin_minus_one = alphaMin - 1;
                static constexpr nlp_numerical_type log_alphaMax_minus_alphaMin_plus_1 = std::log(alphaMax - alphaMin_minus_one);
                nlp_numerical_type theConstant = 0.0; ///< log(alpha_max - alpha_min + 1) / init
            };

            template<typename nlp_numerical_type, IndexType alphaIdx>
            struct alpha_update_trait<exponential_by_obj<nlp_numerical_type, alphaIdx>> 
            {
                typedef exponential_by_obj<nlp_numerical_type, alphaIdx> update_type;

                template<typename nlp_type>
                static constexpr typename nlp_type::nlp_numerical_type obj(nlp_type &nlp) 
                { 
                    switch(alphaIdx)
                    {
                        case 0: return nlp._objHpwlRaw; break;
                        case 1: return nlp._objOvlRaw; break;
                        default: return nlp._objOobRaw; break;
                    }
                }

                template<typename nlp_type>
                static constexpr update_type construct(nlp_type &, alpha_hpwl_ovl_oob<nlp_numerical_type> &) { return update_type(); }

                template<typename nlp_type>
                static constexpr void init(nlp_type &nlp, alpha_hpwl_ovl_oob<nlp_numerical_type> &alpha, update_type &update) 
                { 
                    if (obj(nlp) < REAL_TYPE_TOL)
                    {
                        update.theConstant = -1;
                        DBG("alpha idx %d size %d \n", alphaIdx, alpha._alpha.size());
                        alpha._alpha[alphaIdx] = update.alphaMax;
                        return;
                    }
                    update.theConstant = update.log_alphaMax_minus_alphaMin_plus_1 / obj(nlp);
                    alpha._alpha[alphaIdx] = update.alphaMax;
                }

                template<typename nlp_type>
                static constexpr void update(nlp_type &nlp, alpha_hpwl_ovl_oob<nlp_numerical_type> &alpha, update_type &update) 
                { 
                    if (update.theConstant < REAL_TYPE_TOL)
                    {
                        return;
                    }
                    if (obj(nlp) < REAL_TYPE_TOL)
                    {
                        alpha._alpha[alphaIdx] = update.alphaMax;
                        return;
                    }
                    alpha._alpha[alphaIdx] = std::exp(update.theConstant * obj(nlp)) + update.alphaMin - 1;
                    DBG("new alpha idx %d %f \n", alphaIdx, alpha._alpha[alphaIdx]);
                    DBG("obj %f , the const %f \n", obj(nlp), update.theConstant);
                }

            };


        /// @brief a convenient wrapper for combining different types of stop_condition condition. the list in the template will be check one by one and return converge if any of them say so
        template<typename alpha_update_type, typename... others>
        struct alpha_update_list 
        {
            typedef alpha_update_list<others...> base_type;
            alpha_update_type  _update;
            alpha_update_list<others...> _list;
        };

        template<typename alpha_update_type>
        struct alpha_update_list<alpha_update_type>
        {
            alpha_update_type _update;
        };

        template<typename alpha_update_type, typename... others>
        struct alpha_update_trait<alpha_update_list<alpha_update_type, others...>>
        {
            typedef alpha_update_list<alpha_update_type, others...> list_type;
            typedef typename alpha_update_list<alpha_update_type, others...>::base_type base_type;

            template<typename nlp_type, typename alpha_type>
            static list_type construct(nlp_type &n, alpha_type & a)
            {
                list_type list;
                list._update = std::move(alpha_update_trait<alpha_update_type>::construct(n, a));
                list._list = std::move(alpha_update_trait<base_type>::construct(n, a));
                return list;
            }

            template<typename nlp_type, typename alpha_type>
            static void init(nlp_type &n, alpha_type & a, list_type &c)
            {
                alpha_update_trait<alpha_update_type>::init(n, a, c._update);
                alpha_update_trait<base_type>::init(n, a, c._list);
            }

            template<typename nlp_type, typename alpha_type>
            static void update(nlp_type &n, alpha_type &alpha,  list_type &c) 
            {
                alpha_update_trait<alpha_update_type>::update(n, alpha, c._update);
                alpha_update_trait<base_type>::update(n, alpha, c._list);
            }
        };


        template<typename alpha_update_type>
        struct alpha_update_trait<alpha_update_list<alpha_update_type>>
        {
            typedef alpha_update_list<alpha_update_type> list_type;


            template<typename nlp_type, typename alpha_type>
            static void init(nlp_type &n, alpha_type & a, list_type &c)
            {
                alpha_update_trait<alpha_update_type>::init(n, a, c._update);
            }

            template<typename nlp_type, typename alpha_type>
            static list_type construct(nlp_type &n, alpha_type &alpha)
            {
                list_type list;
                list._update = std::move(alpha_update_trait<alpha_update_type>::construct(n, alpha));
                return list;
            }

            template<typename nlp_type, typename alpha_type>
            static void update(nlp_type &n, alpha_type &alpha,  list_type &c) 
            {
                alpha_update_trait<alpha_update_type>::update(n, alpha, c._update);
            }
        };

        } // namespace update
    } // namespace alpha
} //namespace nlp
PROJECT_NAMESPACE_END
