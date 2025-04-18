/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

 #ifndef __DTLMOD_STAGING_TRANSPORT_HPP__
 #define __DTLMOD_STAGING_TRANSPORT_HPP__
 
 #include "dtlmod/Transport.hpp"

 namespace dtlmod {
 
 /// \cond EXCLUDE_FROM_DOCUMENTATION
 class StagingTransport : public Transport {
 
 protected:
   virtual void create_rendez_vous_points() = 0;
 
 public:
  explicit StagingTransport(Engine* engine) : Transport(engine) {}
  virtual ~StagingTransport() = default;

};
 /// \endcond
 
 } // namespace dtlmod
 #endif
 