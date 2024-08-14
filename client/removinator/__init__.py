from removinator import *

def cli():
    import argparse
    from removinator import removinator

    valid_commands = [
        'insert_card',
        'remove_card',
        'get_status',
    ]

    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command')

    parser_insert_card = subparsers.add_parser('insert_card')
    parser_insert_card.add_argument('slot', type=int)

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
    else:
        print(conn.get_status())
