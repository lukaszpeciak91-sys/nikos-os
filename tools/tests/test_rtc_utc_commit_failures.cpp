#include "clock/rtc_utc_commit.hpp"
#include "clock/warsaw_time_rules.hpp"
#include <cassert>
#include <stdexcept>
using namespace nikos::clock;
using namespace nikos::clock::warsaw;

struct Fake {
    enum Failure { PendingNvs, TrustInvalidate, DateWrite, TimeWrite,
                   Readback, WrongReadback, UtcNvs, TrustRestore, None };
    Failure failure = None;
    bool online=true, pending=false, utc_basis=false, trust=true;
    bool trusted_now=true;
    unsigned writes=0;
    int phase=0, crash_after=0;
    void checkpoint() { if(crash_after && ++phase==crash_after) throw std::runtime_error("power loss"); }
    DateTime rtc{2026,7,15,13,40,0};
    bool available() const { return online; }
    bool save_pending() { if(failure==PendingNvs)return false; checkpoint();return true; }
    void mark_pending() { pending=true;utc_basis=false;trusted_now=false; }
    bool invalidate_date_trust() {
        if(failure==TrustInvalidate)return false;
        trust=false;checkpoint();return true;
    }
    bool write_date(const DateTime& t) {
        if(failure==DateWrite)return false;
        rtc.year=t.year;rtc.month=t.month;rtc.day=t.day;++writes;checkpoint();return true;
    }
    bool write_time(const DateTime& t) {
        if(failure==TimeWrite)return false;
        rtc.hour=t.hour;rtc.minute=t.minute;rtc.second=t.second;++writes;checkpoint();
        return true;
    }
    bool readback(DateTime& out) {
        if(failure==Readback)return false;
        out=rtc;
        if(failure==WrongReadback)++out.minute;
        checkpoint();return true;
    }
    bool save_utc_basis() {
        if(failure==UtcNvs)return false;
        utc_basis=true;checkpoint();return true;
    }
    bool restore_date_trust() {
        if(failure==TrustRestore)return false;
        trust=true;checkpoint();return true;
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
    // Simulate abrupt power loss after each completed irreversible action.
    // Next boot must never authorize partially written UTC or retry offset.
    for(int checkpoint=1;checkpoint<=7;++checkpoint) {
        Fake interrupted{};
        interrupted.crash_after=checkpoint;
        bool crashed=false;
        try { (void)commit_utc_checked(target,interrupted); }
        catch(const std::runtime_error&) { crashed=true; }
        assert(crashed);
        if(checkpoint==1) {
            // Pending was durably written before mark_pending() returned.
            // Fresh boot sees Pending and must refuse clock trust.
            assert(!interrupted.utc_basis);
        } else {
            assert(!interrupted.boot_trusted());
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
