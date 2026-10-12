"""Reject lost, duplicate, invented or unexplained requests in partial road products."""

import shapely

from transport_coupled_roadbed_products import validate_request_ledger,shared_public_addition


def verify_request_ledger():
    a=((1,2,1),(3,4,-1));b=((3,4,1),(1,2,-1));unknown=((5,6,1),(7,8,-1))
    pairs={a:(0,1),b:(1,0)}
    proved=dict(request=a);omitted=dict(request=b,reason='no_constructive_candidate')
    validate_request_ledger(pairs,[proved],[omitted])
    validate_request_ledger(pairs,[proved,dict(request=b)],[])
    invalid=[([proved],[]),([proved,proved],[omitted]),([proved],[omitted,omitted]),
             ([proved,dict(request=b)],[omitted]),([proved],[dict(request=unknown,reason='absent')]),
             ([proved],[dict(request=b)])]
    for proofs,unresolved in invalid:
        try:
            validate_request_ledger(pairs,proofs,unresolved)
        except AssertionError:
            continue
        raise AssertionError('invalid request ledger accepted')
    return dict(partial_and_complete_products_checked=True,invalid_ledgers_rejected=len(invalid))


def verify_shared_public_area():
    physical=shapely.box(0,0,4,4)
    addition=shapely.box(1,1,3,3)
    quantum=2.**-42
    assert shared_public_addition({},physical,quantum).is_empty
    assert shared_public_addition(dict(shared_public_addition_wkb=addition.wkb.hex()),physical,quantum).equals(addition)
    for invalid in (shapely.box(3,1,5,3),shapely.LineString([[1,1],[2,2]])):
        try:shared_public_addition(dict(shared_public_addition_wkb=invalid.wkb.hex()),physical,quantum)
        except AssertionError:pass
        else:raise AssertionError('unbacked shared public area accepted')
    return dict(legacy_products_unchanged=True,declared_shared_public_area=True,unbacked_area_rejected=True)


if __name__=='__main__':
    print(verify_request_ledger()|verify_shared_public_area())
