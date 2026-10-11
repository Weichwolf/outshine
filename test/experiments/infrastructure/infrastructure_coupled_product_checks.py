"""Reject lost, duplicate, invented or unexplained requests in partial road products."""

from infrastructure_coupled_roadbed_products import validate_request_ledger


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


if __name__=='__main__':
    print(verify_request_ledger())
