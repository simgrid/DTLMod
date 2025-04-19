/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

 #ifndef __DTLMOD_STAGING_TRANSPORT_HPP__
 #define __DTLMOD_STAGING_TRANSPORT_HPP__
 
 #include "dtlmod/Transport.hpp"
 #include "dtlmod/StagingEngine.hpp"

 namespace dtlmod {
 
 /// \cond EXCLUDE_FROM_DOCUMENTATION
 class StagingTransport : public Transport {
    friend StagingEngine;
    std::unordered_map<std::string, sg4::MessageQueue*> publisher_put_requests_mq_;

 protected:
   virtual void create_rendez_vous_points() = 0;
   virtual void get_requests_and_do_put(sg4::ActorPtr publisher) = 0;

   // Create a message queue to receive request for variable pieces from subscribers
   void set_publisher_put_requests_mq(const std::string& publisher_name)
   {
       publisher_put_requests_mq_[publisher_name] = sg4::MessageQueue::by_name(publisher_name);
   }
   [[nodiscard]] sg4::MessageQueue* get_publisher_put_requests_mq(const std::string& publisher_name) const
   {
     return publisher_put_requests_mq_.at(publisher_name);
   }

 public:
  std::unordered_map<std::string, sg4::ActivitySet> pending_put_requests;
  explicit StagingTransport(Engine* engine) : Transport(engine) {}
  virtual ~StagingTransport() = default;

};
 /// \endcond
 
 } // namespace dtlmod
 #endif
 