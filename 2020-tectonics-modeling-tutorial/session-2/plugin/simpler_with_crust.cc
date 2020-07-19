/*
  Copyright (C) 2011 - 2020 by the authors of the ASPECT code.

  This file is part of ASPECT.

  ASPECT is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  ASPECT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with ASPECT; see the file LICENSE.  If not see
  <http://www.gnu.org/licenses/>.
*/


#include <aspect/material_model/interface.h>
#include <aspect/simulator_access.h>

#include <iostream>
#include <cmath>


namespace aspect
{
  namespace MaterialModel
  {
    using namespace dealii;

    /**
     * A material model similar to the "simpler" material model, but where the
     * viscosity has two different values dependent on whether we are above or
     * below a line at a certain z-value, i.e., representing a crustal layer.
     *
     * @ingroup MaterialModels
     */
    template <int dim>
    class SimplerWithCrust : public Interface<dim>
    {
      public:
        /**
         * Return whether the model is compressible or not. This model is
         * incompressible.
         */
        virtual bool is_compressible () const;

        /**
         * Return a reference value typical of the viscosities that appear in
         * this model. This value is not actually used in the material
         * description itself, but is used in scaling variables to the same
         * numerical order of magnitude when solving linear systems.
         * Specifically, the reference viscosity appears in the factor scaling
         * the pressure against the velocity. It is also used in computing
         * dimension-less quantities. You may want to take a look at the
         * Kronbichler, Heister, Bangerth 2012 paper that describes the
         * design of ASPECT for a description of this pressure scaling.
         */
        virtual double reference_viscosity () const;

        /**
         * Function to compute the material properties in @p out given the
         * inputs in @p in.
         */
        virtual void evaluate(const typename Interface<dim>::MaterialModelInputs &in,
                              typename Interface<dim>::MaterialModelOutputs &out) const;

        /**
         * @name Functions used in dealing with run-time parameters
         * @{
         */
        /**
         * Declare the parameters this class takes through input files.
         */
        static
        void
        declare_parameters (ParameterHandler &prm);

        /**
         * Read the parameters this class declares from the parameter file.
         */
        virtual
        void
        parse_parameters (ParameterHandler &prm);
        /**
         * @}
         */

      private:
        double reference_density;
        double reference_temperature;
        double eta_L;
        double eta_U;
        double jump_height;
        double thermal_expansion_coefficient;
        double reference_specific_heat;
        double thermal_conductivity;
    };



    template <int dim>
    bool
    SimplerWithCrust<dim>::
    is_compressible () const
    {
      return false;
    }



    template <int dim>
    double
    SimplerWithCrust<dim>::
    reference_viscosity () const
    {
      return eta_L;
    }



    template <int dim>
    void
    SimplerWithCrust<dim>::
    evaluate(const typename Interface<dim>::MaterialModelInputs &in,
             typename Interface<dim>::MaterialModelOutputs &out ) const
    {
      for (unsigned int i=0; i<in.n_evaluation_points(); ++i)
        {
          const double z = in.position[i][1];

          if (z>jump_height)
            out.viscosities[i] = eta_U;
          else
            out.viscosities[i] = eta_L;

          out.densities[i] = reference_density * (1.0 - thermal_expansion_coefficient * (in.temperature[i] - reference_temperature));
          out.thermal_expansion_coefficients[i] = thermal_expansion_coefficient;
          out.specific_heat[i] = reference_specific_heat;
          out.thermal_conductivities[i] = thermal_conductivity;
          out.compressibilities[i] = 0.0;
          out.entropy_derivative_pressure[i] = 0.0;
          out.entropy_derivative_temperature[i] = 0.0;
          for (unsigned int c=0; c<in.composition[i].size(); ++c)
            out.reaction_terms[i][c] = 0.0;
        }
    }



    template <int dim>
    void
    SimplerWithCrust<dim>::declare_parameters (ParameterHandler &prm)
    {
      prm.enter_subsection("Material model");
      {
        prm.enter_subsection("Simpler with crust model");
        {
          prm.declare_entry ("Reference density", "3300",
                             Patterns::Double (0),
                             "Reference density $\\rho_0$. Units: $kg/m^3$.");
          prm.declare_entry ("Reference temperature", "293",
                             Patterns::Double (0),
                             "The reference temperature $T_0$. The reference temperature is used "
                             "in the density formula. Units: $K$.");
          prm.declare_entry ("Lower viscosity", "1e20",
                             Patterns::Double (0),
                             "The value of the viscosity $\\eta_L$ in the lower layer. Units: $Pa s$.");
          prm.declare_entry ("Upper viscosity", "1e23",
                             Patterns::Double (0),
                             "The value of the viscosity $\\eta_U$ in the upper layer. Units: $Pa s$.");
          prm.declare_entry ("Jump height", "100000",
                             Patterns::Double (0),
                             "The height at which the viscosity changes. Units: m.");
          prm.declare_entry ("Thermal conductivity", "4.7",
                             Patterns::Double (0),
                             "The value of the thermal conductivity $k$. "
                             "Units: $W/m/K$.");
          prm.declare_entry ("Reference specific heat", "1250",
                             Patterns::Double (0),
                             "The value of the specific heat capacity $c_p$. "
                             "Units: $J/kg/K$.");
          prm.declare_entry ("Thermal expansion coefficient", "2e-5",
                             Patterns::Double (0),
                             "The value of the thermal expansion coefficient $\\alpha$. "
                             "Units: $1/K$.");

        }
        prm.leave_subsection();
      }
      prm.leave_subsection();
    }



    template <int dim>
    void
    SimplerWithCrust<dim>::parse_parameters (ParameterHandler &prm)
    {
      prm.enter_subsection("Material model");
      {
        prm.enter_subsection("Simpler with crust model");
        {
          reference_density          = prm.get_double ("Reference density");
          reference_temperature      = prm.get_double ("Reference temperature");
          eta_L                      = prm.get_double ("Lower viscosity");
          eta_U                      = prm.get_double ("Upper viscosity");
          jump_height                = prm.get_double ("Jump height");
          thermal_conductivity       = prm.get_double ("Thermal conductivity");
          reference_specific_heat    = prm.get_double ("Reference specific heat");
          thermal_expansion_coefficient = prm.get_double ("Thermal expansion coefficient");
        }
        prm.leave_subsection();
      }
      prm.leave_subsection();

      // Declare dependencies on solution variables
      this->model_dependence.viscosity = NonlinearDependence::none;
      this->model_dependence.density = NonlinearDependence::none;
      this->model_dependence.compressibility = NonlinearDependence::none;
      this->model_dependence.specific_heat = NonlinearDependence::none;
      this->model_dependence.thermal_conductivity = NonlinearDependence::none;
    }
  }
}

// explicit instantiations
namespace aspect
{
  namespace MaterialModel
  {
    ASPECT_REGISTER_MATERIAL_MODEL(SimplerWithCrust,
                                   "simpler with crust",
                                   "A material model that is like the ``simpler'' model but "
                                   "has a jump in the viscosity at a specified depth.")
  }
}
