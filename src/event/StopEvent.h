#pragma once
#include "Event.h"
#include <boost/serialization/access.hpp>

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
