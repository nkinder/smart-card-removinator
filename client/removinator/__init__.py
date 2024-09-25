from removinator import *

def cli():
    import argparse
    from removinator import removinator

    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command')

    parser_insert_card = subparsers.add_parser('insert_card')
    parser_insert_card.add_argument('slot', type=int)

    parser_lock_card = subparsers.add_parser('lock_card')
    parser_lock_card.add_argument('slot', type=int)
    parser_unlock_card = subparsers.add_parser('unlock_card')
    parser_unlock_card.add_argument('slot', type=int)

    subparsers.add_parser('remove_card')
    subparsers.add_parser('get_status')

    args = parser.parse_args()

    conn = removinator.Removinator()

    if args.command == 'insert_card':
        print('Switching slot to {}'.format(args.slot))
        conn.insert_card(args.slot)
    elif args.command == 'remove_card':
        print('Removing card from current slot')
        conn.remove_card()
    elif args.command == 'lock_card':
        print('Locking card slot {}'.format(args.slot))
        conn.lock_card(args.slot)
    elif args.command == 'unlock_card':
        print('Unlocking card slot {}'.format(args.slot))
        conn.unlock_card(args.slot)
    else:
        print(conn.get_status())
