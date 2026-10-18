#include "clock/rtc_utc_commit.hpp"
#include "clock/warsaw_time_rules.hpp"
#include <cassert>
using namespace nikos::clock;
using namespace nikos::clock::warsaw;

struct Fake {
    enum Failure { PendingNvs, TrustInvalidate, DateWrite, TimeWrite,
                   Readback, WrongReadback, UtcNvs, TrustRestore, None };
    Failure failure = None;
    bool online=true, pending=false, utc_basis=false, trust=true;
    bool trusted_now=true;
    unsigned writes=0;
    DateTime rtc{2026,7,15,13,40,0};
    bool available() const { return online; }
    bool save_pending() { return failure!=PendingNvs; }
    void mark_pending() { pending=true;utc_basis=false;trusted_now=false; }
    bool invalidate_date_trust() {
        if(failure==TrustInvalidate)return false;
        trust=false;return true;
    }
    bool write_date(const DateTime& t) {
        if(failure==DateWrite)return false;
        rtc.year=t.year;rtc.month=t.month;rtc.day=t.day;++writes;return true;
    }
    bool write_time(const DateTime& t) {
        if(failure==TimeWrite)return false;
        rtc.hour=t.hour;rtc.minute=t.minute;rtc.second=t.second;++writes;
        return true;
    }
    bool readback(DateTime& out) {
        if(failure==Readback)return false;
        out=rtc;
        if(failure==WrongReadback)++out.minute;
        return true;
    }
    bool save_utc_basis() {
        if(failure==UtcNvs)return false;
        utc_basis=true;return true;
    }
    bool restore_date_trust() {
        if(failure==TrustRestore)return false;
        trust=true;return true;
    }
    void mark_trusted_utc() {pending=false;trusted_now=true;}
    // Boot authorization is based only on the persistent basis + date marker.
    bool boot_trusted() const {return utc_basis && trust;}
};

int main() {
    const DateTime local{2026,7,15,13,40,0};
    const auto res=local_to_utc(local);
    assert(res.status==LocalStatus::Unique);
    const DateTime target=res.daylight_utc;
    assert(target.hour==11);
    Fake ok{};
    assert(commit_utc_checked(target,ok));
    assert(ok.boot_trusted() && ok.writes==2);
    const DateTime saved=ok.rtc;
    // Reboot of UTC storage never feeds stored UTC back into migration.
    assert(ok.boot_trusted() && seconds(ok.rtc)==seconds(saved));

    for (int step=Fake::PendingNvs;step<Fake::None;++step) {
        Fake fake{};
        fake.failure=static_cast<Fake::Failure>(step);
        assert(!commit_utc_checked(target,fake));
        if(step==Fake::PendingNvs) {
            assert(!fake.pending && !fake.utc_basis && fake.trust);
        } else {
            assert(fake.pending && !fake.trusted_now);
            assert(!fake.boot_trusted());
        }
    }
    Fake unavailable{};
    unavailable.online=false;
    assert(!commit_utc_checked(target,unavailable) && unavailable.writes==0);
    Fake invalid{};
    assert(!commit_utc_checked({2026,2,30,12,0,0},invalid));
    assert(invalid.writes==0);
    Fake crossover{};
    const DateTime midnight{2026,12,31,23,59,59};
    assert(commit_utc_checked(midnight,crossover));
    assert(local_to_utc({2026,3,29,2,30,0}).status==LocalStatus::Nonexistent);
    assert(local_to_utc({2026,10,25,2,30,0}).status==LocalStatus::Ambiguous);
}
