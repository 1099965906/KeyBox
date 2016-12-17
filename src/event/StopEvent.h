#pragma once
#include "Event.h"
#include <boost/serialization/access.hpp>
#include <boost/serialization/export.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

//	Todo:stm �Ƿ���Ҫɾ��
class StopEvent : public Event {
public:
	virtual void processEvent() override { }

private:
	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar & boost::serialization::base_object<Event>(*this);
	}
};

BOOST_CLASS_EXPORT_KEY(StopEvent)
